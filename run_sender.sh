# num_threads, 4, "Number of sender threads"
# core_offset, 0, "Core ID of the first sender thread"
# frame_duration, 0, "Frame duration in microseconds"
# inter_frame_delay, 0, "Delay between two frames in microseconds"
# server_mac_addr, "ff:ff:ff:ff:ff:ff", "MAC address of the remote Agora server to send data to"

FILE="files/config/ci/tddconfig-sim-ul.json"

./build/sender  --num_threads=1 --core_offset=10 --frame_duration=1000 --enable_slow_start=0 --conf_file=$FILE