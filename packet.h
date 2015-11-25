/*
 * packet.h
 *
 *  Created on: Nov 25, 2015
 *      Author: vikasgoel
 */

#ifndef PACKET_H_
#define PACKET_H_

struct packet_header {
	char type;
	unsigned int sequence;
	unsigned int length;
}__attribute__((__packed__));



#endif /* PACKET_H_ */
