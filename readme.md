Dos programas en C que realizan la transferencia de archivos, uno mediante UDP y el otro mediante TCP. 
El programa basado en datagramas UDP implementa un protocolo de transferencia de archivos simple, 
con ACK de parte del servidor y retransmisión de paquetes en caso de pérdida.

###############################################################################3

Two C programs for file transfer, one using UDP and the other using TCP.
The UDP datagram-based program implements a simple file transfer protocol,
with ACK from the server, given that UDP does not guarantee packet delivery.


# compile_tcp:   
             
gcc with flags -lssl -lcrypto 

## udp

run make file   


# Usage

  * ./server portno

  * Usage: ./client hostname port file
  
