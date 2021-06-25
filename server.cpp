#include <cstdlib>
#include <ctime>
#include <cstdio>
#include <netinet/in.h>
#include <iostream>

#include "server.h"
#include "ttftps.h"

void serverLoop(int sockFd, struct sockaddr_in clntAddr, unsigned int
cliAddrLen, std::ofstream& fileOnServer)
{
    const int WAIT_FOR_PACKET_TIMEOUT = 3;
    const int NUMBER_OF_FAILURES = 7;

    // TODO ask lior if we should mtu for data packets, like WRQ
    char buffer[MAX_PCKT_LEN] = {0};
    unsigned int timeoutExpiredCount{0};
    unsigned short lastReceivedBlkNum{0};
    unsigned short blockNum{0};
    unsigned short opcode{0};
    ssize_t recvMsgSize{0}; /* Size of received message */

    fd_set readFds;
    struct timeval timeout{0};
    timeout.tv_sec = WAIT_FOR_PACKET_TIMEOUT;
    int readyToRead;

    do
    {
        do
        {
            do
            {
                // Wait WAIT_FOR_PACKET_TIMEOUT to see if something appears
                // for us at the socket (we are waiting for DATA)
                FD_ZERO(&readFds);
                FD_SET(sockFd, &readFds);

                // Returns 0 if timeout was reached and no message was read
                readyToRead = select(sockFd + 1, &readFds, nullptr, nullptr,
                                     &timeout);

                // TODO ask lior if need to check sys call here because it
                //  wasn't in sheled
                SYS_CALL_CHECK(readyToRead);

                // If there was something at the socket and we are here not
                // because of a timeout
                if(readyToRead > 0)
                {
                    // Read the DATA packet from the socket (at least we hope
                    // this is a DATA packet)

                    recvMsgSize = recvfrom(sockFd, buffer, MAX_PCKT_LEN, 0, (struct
                    sockaddr*)&clntAddr, &cliAddrLen);
                    SYS_CALL_CHECK(recvMsgSize);

                    // TODO ask lior when to use ntohs. This works, but switching indexes
                    //  of buffers and using ntohs also works.
                    opcode = (((unsigned short)buffer[0]) << 8) | buffer[1];
                    //opcode = ntohs(opcode);

                    // TODO ask lior when to use ntohs. This works, but switching indexes
                    //  of buffers and using ntohs also works.
                    blockNum = (((unsigned short)buffer[2])
                            << 8) | buffer[3];
                    //opcode = ntohs(opcode);
                }

                //Time out expired while waiting for data to appear at the
                // socket
                if(readyToRead == 0)
                {
                    //TODO: Send another ACK for the last packet
                    timeoutExpiredCount++;

                    std::cout << "FLOWERROR:number of timeouts is: "
                              << timeoutExpiredCount << std::endl;

                    // Create and send ack
                    ACK ack;
                    ack.opcode = htons(ACK_OPCODE);
                    ack.blockNum = htons(lastReceivedBlkNum);

                    auto ackBytesSent = sendto(sockFd, &ack, ACK_LENGTH, 0,
                                               (struct sockaddr*) &clntAddr,
                                                       cliAddrLen);
                    // Ensure that entire ACK was successfully sent
                    if(ackBytesSent != ACK_LENGTH)
                    {
                        perror("TTFTP_ERROR");
                        exit(-1);
                    }

                    std::cout << "OUT:ACK," << lastReceivedBlkNum << std::endl;
                }
                if (timeoutExpiredCount>= NUMBER_OF_FAILURES)
                {
                    // FATAL ERROR BAIL OUT
                    std::cout << "FLOWERROR:number of failures has exceeded "
                    << NUMBER_OF_FAILURES << " exiting..." << std::endl;
                    // TODO check if this is how we should zonaich process
                    exit(-2);
                }
                // TODO check that this (recvMsgSize == 0) is correct
            }while (recvMsgSize == 0); // TODO: Continue while some socket was
                // ready but recvfrom failed to read the data (ret 0)
            if (opcode != 3) // TODO: We got something else but DATA
            {
                // FATAL ERROR BAIL OUT
                std::cout << "FLOWERROR:unexpected packet type received. " <<
                             "Exiting..." << std::endl;
                // TODO check if this is how we should zonaich process
                exit(-3);
            }
            if (blockNum != lastReceivedBlkNum + 1) // TODO: The incoming block
                // number is not what
                // we have
                    // expected, i.e. this is a DATA pkt but the block number
                    // in DATA was wrong (not last ACK’s block number + 1)
            {
                // FATAL ERROR BAIL OUT
                std::cout << "FLOWERROR:block number received does not match "
                             "last ACK's block number + 1. " <<
                          "Exiting..." << std::endl;
                // TODO check if this is how we should zonaich process
                exit(-4);
            }
        }while (false);
        timeoutExpiredCount = 0;

        // Packet is correct, Data packet - print message
        lastReceivedBlkNum = blockNum;
        std::cout << "IN:DATA, " << blockNum << "," << recvMsgSize << std::endl;

        // Write packet to file on server and print message
        fileOnServer.write(buffer + 4, recvMsgSize - 4); // TODO check that
        // this syscall succeeded

        // TODO - here we printed number of bytes that we INTENDED to write,
        //  NOT how many were actually written. Ask Lior if ok.
        std::cout << "WRITING: " << recvMsgSize - 4 << std::endl;

        //lastWriteSize = fwrite(...); // write next bulk of data
        // TODO: send ACK packet to the client

        // Create and send ack
        ACK ack;
        ack.opcode = htons(ACK_OPCODE);
        ack.blockNum = htons(lastReceivedBlkNum);

        auto ackBytesSent = sendto(sockFd, &ack, ACK_LENGTH, 0,
                                   (struct sockaddr*) &clntAddr,
                                   cliAddrLen);
        // Ensure that entire ACK was successfully sent
        if(ackBytesSent != ACK_LENGTH)
        {
            perror("TTFTP_ERROR");
            exit(-1);
        }

        std::cout << "OUT:ACK," << lastReceivedBlkNum << std::endl;
    }while (recvMsgSize != MAX_PCKT_LEN); // Have blocks left to be read from client
    // (not end of
    // transmission)
}