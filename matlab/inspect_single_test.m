
clc
clear

dataset_filename = "UeRxData-loc1.h5"; % changeable
inspect_frame = 100; % changeable
verbose = "true"; % changeable 
group_id = '/Data'; % fixed

samples_per_slot = double(h5readatt(dataset_filename, group_id, 'SLOT_SAMP_LEN'));
tx_zero_prefix_len = double(h5readatt(dataset_filename, group_id, 'TX_ZERO_PREFIX_LEN'));
data_size = double(h5readatt(dataset_filename, group_id, 'OFDM_DATA_NUM'));
data_start = double(h5readatt(dataset_filename, group_id, 'OFDM_DATA_START'));
data_stop = double(h5readatt(dataset_filename, group_id, 'OFDM_DATA_STOP'));
fft_size = double(h5readatt(dataset_filename, group_id, 'OFDM_CA_NUM'));
cp_len = double(h5readatt(dataset_filename, group_id, 'CP_LEN'));
total_dl_symbols = double(h5readatt(dataset_filename, group_id, 'DL_SLOTS'));
dl_pilot_symbols = double(h5readatt(dataset_filename, group_id, 'DL_PILOT_SLOTS'));
total_users = 1;
beacon_syms = 1;

configs = [samples_per_slot tx_zero_prefix_len data_size data_start data_stop fft_size cp_len ...
    total_dl_symbols dl_pilot_symbols total_users]

%% CHOOSE THE DOWNLINK DATA AND DO PROCESSING
dataset_id = '/DownlinkData';  % fixed, but will be updated below to process different data
% Dimensions  [Samples, Ant, Symbol, Cells, Frame]
start = [1 1 1 1 inspect_frame];
count = [(samples_per_slot * 2) total_users total_dl_symbols 1 1];   % only do processing on data 'D's
%Display Info
if verbose == "true"
    h5disp(dataset_filename,strcat(group_id,dataset_id));  % show info of downlink dataset
end
%Generate a int16 array
rx_syms_hdf5 = h5read(dataset_filename, strcat(group_id,dataset_id), start, count);
%Convert to double and scale
rx_syms_scaled_double = double(rx_syms_hdf5) ./ double(intmax('int16'));
% Convert to complex double
% Samples x User x Symbol
rx_syms_cxdouble = complex(rx_syms_scaled_double(1:2:end,:,:), rx_syms_scaled_double(2:2:end,:, :));
rx_pilot_cxdouble = rx_syms_cxdouble(:,:,1:dl_pilot_symbols);
rx_data_cxdouble = rx_syms_cxdouble(:,:,1+dl_pilot_symbols:end);


%% CHOOSE THE BEACON DATA AND DO PROCESSING
dataset_id = '/BeaconData';
% Dimensions  [Samples, Ant, Symbol, Cells, Frame]
start = [1 1 1 1 inspect_frame];
count = [(samples_per_slot * 2) total_users beacon_syms 1 1];  % only do processing on beacon
%Display Info
if verbose == "true"
    h5disp(dataset_filename,strcat(group_id,dataset_id));
end
%Generate a int16 array
rx_beacon_hdf5 = h5read(dataset_filename, strcat(group_id,dataset_id), start, count);
%Convert to double and scale
rx_beacon_scaled_double = double(rx_beacon_hdf5) ./ double(intmax('int16'));
%Convert to complex double
% Samples x User x Symbol
rx_beacon_cxdouble = complex(rx_beacon_scaled_double(1:2:end,:,:), rx_beacon_scaled_double(2:2:end,:, :));
rx_beacon_rssi = process_beacon(rx_beacon_cxdouble, tx_zero_prefix_len);  % CALL BEACON func.


%% CHOOSING THE TXPILOT AND DO PROCESSING
dataset_id = '/TxPilot';
total_samples = data_size * 2; %*2 for complex type (native float)
if verbose == "true"
    h5disp(dataset_filename,strcat(group_id,dataset_id));
end
start = [1 1 1 1 1];
count = [total_samples total_users 1 1 1];
tx_pilot_hdf5 = double(h5read(dataset_filename, strcat(group_id,dataset_id), start, count));
%Convert to complex
tx_pilot_cxdouble = complex(tx_pilot_hdf5(1:2:end,:), tx_pilot_hdf5(2:2:end,:));


%% CHOOSING THE TXDATA AND DO PROCESSING
dataset_id = '/TxData';
%Compare the pilot data,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
if verbose == "true"
    h5disp(dataset_filename,strcat(group_id,dataset_id));
end
start = [1 1 1 1 1];
count = [total_samples total_users total_dl_symbols 1 1];
tx_data_hdf5 = double(h5read(dataset_filename, strcat(group_id,dataset_id), start, count));
%Convert to complex type
tx_syms_cxdouble = complex(tx_data_hdf5(1:2:end,:,:), tx_data_hdf5(2:2:end,:,:));
% Samples (complex) x User Ant x Downlink Symbol Id
% removing the pilot for now because it doesn't check out?
tx_pilot_bad = tx_syms_cxdouble(:,:,1);
tx_data_cxdouble = tx_syms_cxdouble(:,:,2:end);
disp(isequal(tx_pilot_bad, tx_pilot_cxdouble));

%% PROCESSING THE RECEIVE FRAMES
[demul_data, data_sc_idx, evm, snr] = ...
    process_rx_frame(configs, tx_pilot_cxdouble, tx_data_cxdouble, rx_pilot_cxdouble, rx_data_cxdouble);

% ignore plooting OTA time samples
% plot constellation points
for u=1:total_users
    rx_cnstl = demul_data(data_sc_idx, : , u);
    tx_cnstl = tx_data_cxdouble(data_sc_idx, :, u);
    figure('Name', ['Constellation [User ', num2str(u), ']']);
    pt_size = 15;
    scatter(real(rx_cnstl(:)), imag(rx_cnstl(:)),pt_size,'r','filled');
    hold on
    pt_size = 250;
    scatter(real(tx_cnstl(:)), imag(tx_cnstl(:)),pt_size, 'b', 'p', 'filled');
    title(['Constellation [User ', num2str(u), ', Frame ', num2str(inspect_frame), ']']);
end

disp(['Frame Inspect: ', num2str(inspect_frame)]);
disp(['Beacon RSSI (dB): ', num2str(rx_beacon_rssi)]);
disp(['SNR: ', num2str(snr)]);
disp(['EVM: ', num2str(evm)]);






















