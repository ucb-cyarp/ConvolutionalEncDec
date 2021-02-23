clear; close all; clc;

overSample =4;
awgnSNR = -4:0.25:35;
EsN0 = awgnSNR + 10*log10(overSample);

%Code Spec
constrLen = 7;
generators = [133, 171];

genSize = size(generators);
codeRate = length(constrLen)/genSize(2);
trellis = poly2trellis(constrLen, generators);
codeDistanceSpec = distspec(trellis, 10);

codeLbl = ['Coded - Rate: ' num2str(codeRate) ', Constr Len: ' num2str(constrLen) ', Hard' ];
codeLblSoft = ['Coded - Rate: ' num2str(codeRate) ', Constr Len: ' num2str(constrLen) ', Soft'  ];

%uncoded
infoBitsPerSymbol64Uncoded = log2(64); %Change when coding introduced
infoBitsPerSymbol16Uncoded = log2(16); %Change when coding introduced
infoBitsPerSymbolBPSKUncoded = log2(2); %Change when coding introduced
infoBitsPerSymbolQPSKUncoded = log2(4); %Change when coding introduced
EbN064Uncoded = EsN0 - 10*log10(infoBitsPerSymbol64Uncoded);
EbN016Uncoded = EsN0 - 10*log10(infoBitsPerSymbol16Uncoded);
EbN0BPSKUncoded = EsN0 - 10*log10(infoBitsPerSymbolBPSKUncoded);
EbN0QPSKUncoded = EsN0 - 10*log10(infoBitsPerSymbolQPSKUncoded);
idealBerBPSKUncoded = berawgn(EbN0BPSKUncoded, 'psk', 2, 'nondiff');
idealBerQPSKUncoded = berawgn(EbN0QPSKUncoded, 'psk', 4, 'nondiff');
idealBer16QAMUncoded = berawgn(EbN016Uncoded, 'qam', 16, 'nondiff');
idealBer64QAMUncoded = berawgn(EbN016Uncoded, 'qam', 64, 'nondiff');

%coded
%See https://www.mathworks.com/help/comm/ref/bercoding.html
%https://www.mathworks.com/help/comm/ref/distspec.html
%https://www.mathworks.com/help/comm/ug/awgn-channel.html
%https://www.mathworks.com/help/comm/ref/poly2trellis.html
infoBitsPerSymbol64Coded = log2(64)*codeRate; %Change when coding introduced
infoBitsPerSymbol16Coded = log2(16)*codeRate; %Change when coding introduced
infoBitsPerSymbolBPSKCoded = log2(2)*codeRate; %Change when coding introduced
infoBitsPerSymbolQPSKCoded = log2(4)*codeRate; %Change when coding introduced
EbN064Coded = EsN0 - 10*log10(infoBitsPerSymbol64Coded);
EbN016Coded = EsN0 - 10*log10(infoBitsPerSymbol16Coded);
EbN0BPSKCoded = EsN0 - 10*log10(infoBitsPerSymbolBPSKCoded);
EbN0QPSKCoded = EsN0 - 10*log10(infoBitsPerSymbolQPSKCoded);
idealBerBPSKCoded = bercoding(EbN0BPSKCoded, 'conv', 'hard', codeRate, codeDistanceSpec, 'psk', 2, 'nondiff');
idealBerQPSKCoded = bercoding(EbN0QPSKCoded, 'conv', 'hard', codeRate, codeDistanceSpec, 'psk', 4, 'nondiff');
idealBer16QAMCoded = bercoding(EbN016Coded, 'conv', 'hard', codeRate, codeDistanceSpec, 'qam', 16, 'nondiff');
idealBer64QAMCoded = bercoding(EbN016Coded, 'conv', 'hard', codeRate, codeDistanceSpec, 'qam', 64, 'nondiff');
idealBerBPSKCodedSoft = bercoding(EbN0BPSKCoded, 'conv', 'soft', codeRate, codeDistanceSpec, 'psk', 2, 'nondiff');
idealBerQPSKCodedSoft = bercoding(EbN0QPSKCoded, 'conv', 'soft', codeRate, codeDistanceSpec, 'psk', 4, 'nondiff');

figure;
subplot(2, 1, 1);
%Uncoded
semilogy(EbN0BPSKUncoded, idealBerBPSKUncoded, 'k-');
hold on;
semilogy(EbN0QPSKUncoded, idealBerQPSKUncoded, 'r--');
semilogy(EbN016Uncoded, idealBer16QAMUncoded, 'b-');
semilogy(EbN064Uncoded, idealBer64QAMUncoded, 'g-');
%Coded
semilogy(EbN0BPSKCoded, idealBerBPSKCoded, 'k.--');
semilogy(EbN0QPSKCoded, idealBerQPSKCoded, 'ro--');
semilogy(EbN016Coded, idealBer16QAMCoded, 'b.--');
semilogy(EbN064Coded, idealBer64QAMCoded, 'g.--');
semilogy(EbN0BPSKCoded, idealBerBPSKCodedSoft, 'k*:');
semilogy(EbN0QPSKCoded, idealBerQPSKCodedSoft, 'rs:');
legend({'BPSK (Uncoded)', 'QPSK (Uncoded)', '16QAM (Uncoded)', '64QAM (Uncoded)', ...
    ['BPSK (' codeLbl ')'], ['QPSK (' codeLbl ')'], ['16QAM (' codeLbl ')'], ['64QAM (' codeLbl ')'] ...
    ['BPSK (' codeLblSoft ')'], ['QPSK (' codeLblSoft ')']}, 'location', 'southwest');
xlabel('EbN0');
ylabel('BER');
title('BER vs. EbN0');
grid on;

subplot(2, 1, 2);
%Uncoded
semilogy(awgnSNR, idealBerBPSKUncoded, 'k-');
hold on;
semilogy(awgnSNR, idealBerQPSKUncoded, 'r-');
semilogy(awgnSNR, idealBer16QAMUncoded, 'b-');
semilogy(awgnSNR, idealBer64QAMUncoded, 'g-');
%Coded
semilogy(awgnSNR, idealBerBPSKCoded, 'k.--');
semilogy(awgnSNR, idealBerQPSKCoded, 'ro--');
semilogy(awgnSNR, idealBer16QAMCoded, 'b.--');
semilogy(awgnSNR, idealBer64QAMCoded, 'g.--');
legend({'BPSK (Uncoded)', 'QPSK (Uncoded)', '16QAM (Uncoded)', '64QAM (Uncoded)', ['BPSK (' codeLbl ')'], ['QPSK (' codeLbl ')'], ['16QAM (' codeLbl ')'], ['64QAM (' codeLbl ')']}, 'location', 'southwest');
xlabel('SNR');
ylabel('BER');
title('BER vs. SNR');
grid on;

%% Compute a BER for a set of points we can test
% Note that, when coding, the EbN0 changes because the number of
% information bits per symbol goes down.  When gettting the BER of the
% uncoded channel, we want to maintain the same SNR or same EsN0
% (same error probability for symbols whether they are coded or not).
constrLen = 7;
generators = [133, 171];

genSize = size(generators);
codeRate = length(constrLen)/genSize(2);
trellis = poly2trellis(constrLen, generators);
codeDistanceSpec = distspec(trellis, 10);

codeLbl = ['Coded - Rate: ' num2str(codeRate) ', Constr Len: ' num2str(constrLen) ', Hard' ];
codeLblSoft = ['Coded - Rate: ' num2str(codeRate) ', Constr Len: ' num2str(constrLen) ', Soft'  ];

SNR = [-4,-3,-2];
overSample =4;
EsN0 = SNR + 10*log10(overSample);
infoBitsPerSymbolBPSKUncoded = log2(2); %Change when coding introduced
EbN0BPSKUncoded = EsN0 - 10*log10(infoBitsPerSymbolBPSKUncoded);

infoBitsPerSymbolBPSKCoded = log2(2)*codeRate;
EbN0BPSKCoded = EsN0 - 10*log10(infoBitsPerSymbolBPSKCoded);

berBPSKUncoded = berawgn(EbN0BPSKUncoded, 'psk', 2, 'nondiff');
berBPSKCoded = bercoding(EbN0BPSKCoded, 'conv', 'hard', codeRate, codeDistanceSpec, 'psk', 2, 'nondiff');

fprintf(' SNR    Uncoded BER      Coded BER\n');
for i = 1:length(SNR)
    fprintf('%4d  %13e  %13e\n', SNR(i), berBPSKUncoded (i), berBPSKCoded(i));
end

%% Compute BER for a variety of distspec entries

constrLen = 7;
generators = [133, 171];

genSize = size(generators);
codeRate = length(constrLen)/genSize(2);
trellis = poly2trellis(constrLen, generators);

SNR = -2;
overSample =4;
EsN0 = SNR + 10*log10(overSample);
infoBitsPerSymbolBPSKCoded = log2(2)*codeRate;
EbN0BPSKCoded = EsN0 - 10*log10(infoBitsPerSymbolBPSKCoded);

specPts = 1:1:20;

berBPSKCoded = zeros(size(specPts));
for i = 1:length(specPts)
    codeDistanceSpec = distspec(trellis, specPts(i));
    berBPSKCoded(i) = bercoding(EbN0BPSKCoded, 'conv', 'hard', codeRate, codeDistanceSpec, 'psk', 2, 'nondiff');
end

fprintf('distspecParam      Coded BER\n');
for i = 1:length(specPts)
    fprintf('%13d  %13e\n', specPts(i), berBPSKCoded(i));
end