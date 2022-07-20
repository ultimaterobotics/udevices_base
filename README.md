# udevices_base
Base USB receiver unit for uDevices. Implements custom multi-device protocol to receive data (in certain radio mode) from uECG, uEMG, uMyo and other (not yet made public) projects. Data still have to be properly parsed for each device separately.

For any uDevice which uses function star_queue_send() with compatible parameters (as of now, all are compatible although that could be changed in a particular project at some point) USB receiver gets those packets and sends them unchanged via USB with adding 3 bytes before each one: 0x4F, 0xD5 and rssi level measured when receiving this packet. Two prefix bytes are used to ensure stream sync when parser software first attaches to the data stream or if some corruption of USB data happened
