%% Viterbi BER Estimate

%Based on the example from https://www.mathworks.com/help/comm/ref/vitdec.html

%Being used as an alternative to bercoding due to variations with the
%number of terms in the distance spectrum discovered and other
%approximations.

%Code Params
constrLen = 7;
generators = [133, 171];

genSize = size(generators);
codeRate = length(constrLen)/genSize(2);
trellis = poly2trellis(constrLen, generators);

tracebackLen = constrLen*5;

%Pkt Params
pkts = 1000;
pktLenBytes = 10240;
pktLenBits = pktLenBytes*8;

%Channel Params
SNR = -5:1:-3;
overSample = 4;
EsN0 = SNR + 10*log10(overSample);
M = 2;

if M ~= 2 || log2(trellis.numOutputSymbols) ~= 2
    error('Currently only support M=2 for splitting/combining symbols and 2 output sybols');
end

infoBitsPerSymbolUncoded = log2(M);
EbN0Uncoded = EsN0 - 10*log10(infoBitsPerSymbolUncoded);
expectedUncodedBer = berawgn(EbN0Uncoded, 'psk', M, 'nondiff');

infoBitsPerSymbolCoded = log2(M)*codeRate;
EbN0Coded = EsN0 - 10*log10(infoBitsPerSymbolCoded);

%Modulation Scale Factor
%See help modnorm
constPts = pskmod(0:(M-1), M);
scaleFactor = modnorm(constPts, 'avpow', 1);

%Simulate
fprintf(' SNR  Expected Uncoded BER     Uncoded BER      Coded BER\n');
for i = 1:length(SNR)
    uncodedBitsSent = 0;
    uncodedBitErrors = 0;
    decodedBitsRecieved = 0;
    decodedBitErrors = 0;
    for pkt = 1:pkts
        %Create Pkt
        pktOrig = [randi(M, [1, pktLenBits])-1, zeros(1, constrLen-1)]; %Force the encoder back to the 0 state
        
        %Encode
        %NOTE: The encoder outputs binary data and the output array
        %is not the same size as the input array.  The documentation
        %suggests differently, that each elements contains multiple bits.
        %However, looking at the example for vitdec shows that this is not
        %the case as the modulator is envoked in 'bit' mode and the 
        %viterbi decoder is run with the 'hard' decision mode which the
        %documentation identifies as meaning 
        [pktEncoded, state] = convenc(pktOrig, trellis);
        if state ~= 0
            error('Encoder should have returned to all zero state');
        end
        
        %Modulate
        pktModulated = pskmod(pktEncoded, M).*scaleFactor; %Scale factor should be 1 for PSK
        
        %AWGN Channel Corrupt
        %Note, awgn takes in the SNR.  For a 1 sample/symbol system, this 
        %is equivalent to EsN0.  However, with oversampling, the EsN0 and 
        %SNR differ.
        %We want the effective SNR for 1 sample/symbol which has the same
        %EsN0 as the signal with SNR with multiple samples/symbol.
        %Since, EsN0 = SNR for the case of 1 sample/symbol, we directly use
        %the EsN0 computed from the 

        %Looks like we do not need to normalize the modulation before
        %passing to awgn for PSK, see eample in 
        %https://www.mathworks.com/help/comm/ref/pskmod.html
        %The option 'measured' can be used where AWGN will estimate the
        %input signal power.  The modnorm function can also be used to
        %normalize the modulated signal for average or peak power.
        pktAWGN = awgn(pktModulated, EsN0(i), 0, 'db')./scaleFactor; %0dBW = 1W, Scale factor should be 1 for PSK
        
        %Demodulate
        pktDemodulated = pskdemod(pktAWGN, M);
       
        %Check Channel Error Rate (Uncoded Rate)
        [bitErrsUncoded, bitErrRateUncoded] = biterr(pktEncoded, pktDemodulated);
        uncodedBitsSent = uncodedBitsSent+length(pktEncoded);
        uncodedBitErrors = uncodedBitErrors + bitErrsUncoded;
        
        %Decode
        decodedPkt = vitdec(pktDemodulated,trellis,tracebackLen,'term','hard'); %Term specifies that the message is padded with zeros so that the convolutional encoder ends in the zero state
        %Will only check the actual bits and not the padded bits which
        %returns the convolutional encoder state to 0
        [bitErrsCoded, bitErrRateCoded] = biterr(pktOrig(1:pktLenBits), decodedPkt(1:pktLenBits));
        decodedBitsRecieved = decodedBitsRecieved + pktLenBits;
        decodedBitErrors = decodedBitErrors + bitErrsCoded;
    end
    fprintf('%4d  %20e   %13e  %13e [%d/%d]\n', SNR(i), expectedUncodedBer(i),  uncodedBitErrors/uncodedBitsSent, decodedBitErrors/decodedBitsRecieved, decodedBitErrors, decodedBitsRecieved);

end