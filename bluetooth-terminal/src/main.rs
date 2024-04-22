use bluetooth_serial_port::{BtAddr, BtDevice, BtProtocol, BtSocket};
use std::io::{stdout, Read, Write};
use std::str::FromStr;
use text_io::read;

fn main() {
    // Create a device to connect to
    let device = BtDevice::new(
        String::from("UNL-Rover"),
        BtAddr::from_str("08:F9:E0:ED:20:4A").unwrap()
    );

    loop {
        // Connect to the bluetooth device
        print!("Connecting to {}...\r", device.addr.to_string());
        stdout().flush().unwrap();
        let mut socket = BtSocket::new(BtProtocol::RFCOMM).unwrap();
        socket.connect(device.addr).unwrap();
        println!("Connected to {}... Console ready", device.addr.to_string());

        // Create a simple console
        bt_console(&mut socket);

        println!("Trying to reconnect...");
    }
}

fn bt_console(socket: &mut BtSocket) {
    loop {
        print!("$ ");
        let mut buffer: String = read!("{}\n");
        buffer.push_str("\n");

        // Write the command to the device, if it fails, try to reconnect
        let out_size = match socket.write(&buffer.as_bytes()[..]) {
            Ok(size) => size,
            Err(err) => {
                println!("Error: {}", err);
                break
            },
        };

        let mut buf = [0; 1];
        match socket.read(&mut buf) {
            Ok(size) => size,
            Err(err) => {
                println!("Error: {}", err);
                break
            },
        };

        if 0b10000000 & buf[0] != 0 {
            let zeroed = 128 - (buf[0] & 0b01111111);
            if zeroed == 1 {
                println!("Invalid command");
            } else if zeroed == 2 {
                println!("Expected another argument");
            } else {
                println!("Error! Returned {}, expected {}", zeroed, out_size);
            }
        }
    }
}
