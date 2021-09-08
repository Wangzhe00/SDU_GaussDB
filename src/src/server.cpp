#include <stdio.h>
#include <unistd.h>
#include <netdb.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include <sys/un.h>
#include <stdint.h>
#include <cerrno>
#include <cstdlib>

#define _6K 6*1024
#define BUF_SIZE  (16 * 1024)
 
#define errlog(errmsg) do{perror(errmsg);\
                          printf("%s-->%s-->%d\n", __FILE__, __func__, __LINE__);\
                          exit(1);}\
                          while(0)

uint8_t recvBuf[BUF_SIZE];

enum MSG_TYPE {
  GET = 0,
  SET,
  INVALID_TYPE
};

struct __attribute__((packed)) Header {
  unsigned char msg_type;
  unsigned int page_no;
  unsigned int page_size;
};

static int read_loop(int fd, unsigned char *buf, uint count) {
	int ret = count;
	while (count) {
		size_t recvcnt = read(fd, buf, count);
		if (recvcnt == -1) {
			if (errno == EINTR) {
				continue;
			}
			printf("Package read error.\n");
			return -1;
		} else if (recvcnt == 0) {
			printf("Connection disconnected.\n");
			return 0;
		}

		count -= recvcnt;
		buf += recvcnt;
	}
	return ret;
}

static int write_loop(int fd, unsigned char *buf, uint count) {
	int ret = count;
	while (count) {
		size_t sendcnt = write(fd, buf, count);
		if (sendcnt == -1) {
			if (errno == EINTR) {
				continue;
			}
			printf("Package read error.\n");
			return -1;
		} else if (sendcnt == 0) {
			printf("Connection disconnected.\n");
			return 0;
		}
		count -= sendcnt;
		buf += sendcnt;
	}
	return ret;
}

int main(int argc, char **argv)
{
    int iClientSockfd;
    int iRet;
    char sendBuf[BUF_SIZE] = "The Data from Unix Client for Unix test...";
    ssize_t recvRet, sendRet;
    struct sockaddr_un clientAddr;
    socklen_t addrLen = sizeof(clientAddr);
 
    bzero(&clientAddr, addrLen);
 
    printf("UNIX CLIENT Start########################\n");
 
    iClientSockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(iClientSockfd < 0){
        errlog("socket error\n");
    }
 
    clientAddr.sun_family = AF_UNIX;
    strcpy(clientAddr.sun_path, "/tmp/sockfile.sock");
 
    iRet = connect(iClientSockfd, (struct sockaddr *)&clientAddr, addrLen);
    if((iRet < 0) && (errno != EINPROGRESS)){
        errlog("connect error\n");
    }
 
    printf("connect successfully, ready to send data*************\n");
 
    while(1) {
		for (unsigned int i = 0; i < 1024; ++i) {
			struct Header header = {GET, i, _6K};
			sendRet = write_loop(iClientSockfd, (uint8_t *) &header, sizeof(header));
			if(sendRet < 0) {
				errlog("write error\n");
			}
	 
			printf("client is waitting data from server...\n");
			// sleep(1);
			recvRet = read_loop(iClientSockfd, (uint8_t *) &header.page_size, sizeof(header.page_size));
			if(recvRet != sizeof(header.page_size)){
				printf("read error\n");
				continue;
			}
			recvRet = read_loop(iClientSockfd, recvBuf, header.page_size);
			if(recvRet != header.page_size){
				printf("read error\n");
			}
            printf("recv Size = [%d]\n", header.page_size);
            for (int i = 0; i < 0x10; ++i) {
                printf("%02X ", recvBuf[i]);
            }
            puts("");
		}
    }
    close(iClientSockfd);
}