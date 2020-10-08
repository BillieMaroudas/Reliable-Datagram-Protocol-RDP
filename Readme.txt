Reliable Datagram Protocol (RDP)
================================

Brief Assignment Description
-----------------------
The purpose of this assignment is to complete the design of the RDP protocol and implement it using the datagram (DGRAM) socket Application Programming Interface (API) with UDP. The learning goal is to implement a reliable protocol on top of the unreliable UDP socket.

The assignment is consisted by two parts: 
a) a receiving server program 
b) a sending client program

The RDP is able to transfer a file of any size from the sender to athe receiver, through a router, although for file sizes over 1MB it is a little slow. The max packet size (including RDP packet header and data payload)is 1,024 bytes.They have been included error control procedures for the following events:
1) Packet creation of unknown type
2) Failure to create UDP socket 
3) Failure to bind to socket 
4) Bad sender or receiver address
5) Failure to open the file
6) Timeout failure

The sender has the following command syntax:

./rdps sender_ip sender_port receiver_ip receiver_port sender_file_name

where
sender_ip: the IP address the sender will use
sender_port: the UDP port number the sender will use
receiver_ip: the IP address the receiver will use
receiver_port: the UDP port number the receiver will use
sender_file_name: the location of the file to be sent

While transferring the file, the RDP sender prints a log message with the following format:

HH:MM:SS.us event_type sip:spt dip:dpt packet_type seqno/ackno length/window86

The sender has the following command syntax:

./rdpr receiver_ip receiver_port receiver_file_name

where
receiver_file_name: the location where the reeived file is stored

After the file transfer is finished, the RDP receiver/RDP sender print a summary message with the following format:
total data bytes received/sent:
unique data bytes received/sent:
total data packets received/sent:
unique data packets received/sent:
SYN packets received/sent:
FIN packets received/sent:
RST packets received/sent:
ACK packets sent/received:
RST packets sent/received:
total time duration (second):
