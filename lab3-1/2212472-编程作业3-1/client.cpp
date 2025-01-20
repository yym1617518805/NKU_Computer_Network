#include<fstream>
#include<iostream>
#include <iostream>
#include<WinSock2.h>
#include<time.h>
#include<stdio.h>
#include<cmath>
#include<algorithm>
#include<string>
#pragma warning(disable:4996)
#pragma comment(lib,"ws2_32.lib")
#define  Max_Size 8192  //每一次传输的文件的字节数
#define MAX_TIME 1000  //多长时间之后我们需要重传
using namespace std;

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned long DWORD;

struct message {
	WORD source_port = 0, dest_port = 0;	//port
	DWORD seq_num = 0, ack_num = 0;			//seq,ack
	WORD  length = 0;					//length
	BYTE   flag = 0;				     //flag
	WORD checksum = 0;					//校验和
	char msg[Max_Size] = { 0 };
};//报文

struct pseudoHead {
	DWORD source_ip = 0, dest_ip = 0;		//ip
	char zero = 0;
	char protocol = 0;
	WORD length = sizeof(message);
}; //伪首部

//下面就是设置message的结构体的信息
void setlength(message* message, short int len) {
	message->length = len & 0x0004;
}
short int getlength(message& message) {
	return message.length;
}

//ACK=0x01, SYN=0x02, FIN=0x04, END=0x08 
//设置状态，END在后面会表示最后一个包的传输。
void cleanflag(message* message) {
	message->flag &= 0x0;
}//清空标志位
void setAck(message* message) {
	message->flag |= 0x01;
}//将标志位设置为ACK，表示确认。
bool isAck(message* message) {
	return message->flag & 0x01;
}//判断是不是Ack（接收）的模式
void setSyn(message* message) {
	message->flag |= 0x02;
}//设置flag标志位为建立链接
bool isSyn(message* message) {
	return message->flag & 0x02;
}//是不是建立链接的状态。
void setFin(message* message) {
	message->flag |= 0x04;
}//将flag设置为关闭连接
bool isFin(message* message) {
	return message->flag & 0x04;
}//判断发送的信息是不是想要关闭连接
void setEnd(message* message) {
	message->flag |= 0x08;
}//在标准的规范里添加的状态，表示是不是最后一个数据包的传输
bool isEnd(message* message) {
	return message->flag & 0x08;
}//判断是不是最后一个包
void setMsg(message* message, char* data) {
	memcpy(message->msg, data, Max_Size);
}//就是将我们当前读取的数据进行拷贝;
void setChecksum(message* messages, pseudoHead* ph) {
	//设为0
	messages->checksum = 0;
	int sum = 0;
	int len_pseudo = sizeof(pseudoHead);
	int len_msg = sizeof(message);
	for (int i = 0; i < len_pseudo / 2; i++) {
		sum += ((WORD*)ph)[i];
	}
	for (int i = 0; i < len_msg / 2; i++) {
		sum += ((WORD*)messages)[i];
	}
	while (sum >> 16) {
		sum = (sum & 0xffff) + (sum >> 16);
	}
	//就是将所有的数据按WORD(十六位)相加，超过十六位的数字平移到末尾再加上。
	//最后对结果取反就成为校验位。
	messages->checksum = ~sum;

};
bool verfiyChecksum(message* messages, pseudoHead* ph) {
	int sum = 0;
	int len_pseudo = sizeof(pseudoHead);
	int len_msg = sizeof(message);
	for (int i = 0; i < len_pseudo / 2; i++) {
		sum += ((WORD*)ph)[i];
	}
	for (int i = 0; i < len_msg / 2; i++) {
		sum += ((WORD*)messages)[i];
	}
	while (sum >> 16) {
		sum = (sum & 0xffff) + (sum >> 16);
	}
	//相同的操作构造出来sum，同时不取反，那么我们和取反的结果相加，肯定十六位数字全部为1.
	return sum == 0xffff;
};


SOCKET sockClient;
SOCKADDR_IN addr_client;
SOCKADDR_IN addr_server;


//下面就是对于ip以及端口号的初始化
char  ip_client[] = "127.0.0.1";
WORD  port_client = 8079;
char ip_server[] = "127.0.0.1";
WORD  port_server = 8080;
char ip_route[] = "127.0.0.1";
WORD  port_route = 8081;

pseudoHead ph;		//伪首部

void start() {
	//Startup
	WSADATA wsadata;
	WORD version;
	version = MAKEWORD(2, 2);
	int result_start;
	result_start = WSAStartup(version, &wsadata);
	if (result_start != 0) {
		cout << "Startup failed" << endl;
		return;
	}
	addr_client.sin_port = htons(port_client);					//port
	addr_client.sin_addr.S_un.S_addr = inet_addr(ip_client); //ip addr
	addr_client.sin_family = AF_INET;

	addr_server.sin_port = htons(port_route);
	addr_server.sin_addr.S_un.S_addr = inet_addr(ip_route);
	addr_server.sin_family = AF_INET;



	//creat socket
	sockClient = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockClient == INVALID_SOCKET) {
		cout << "socket creat failed" << endl;
		return;
	}

	u_long imode = 1;
	ioctlsocket(sockClient, FIONBIO, &imode);//非阻塞

	//bind
	int result_bind;
	result_bind = bind(sockClient, (SOCKADDR*)&addr_client, sizeof(SOCKADDR_IN));
	if (result_bind == SOCKET_ERROR) {
		cout << "bind failed" << endl;
		return;
	}
	//初始化伪头部
	memset(&ph, 0, sizeof(pseudoHead));
	ph.source_ip = inet_addr(ip_client);
	ph.dest_ip = inet_addr(ip_server);
	cout << "初始化成功，客户端开始进行三次握手链接" << endl;
}

bool FoundConnect() {

	int len = sizeof(SOCKADDR_IN);
	char recBuffer[sizeof(message)];	//接收缓冲区
	char sendBuffer[sizeof(message)];	//发送缓冲区
	memset(recBuffer, 0, sizeof(message));
	memset(sendBuffer, 0, sizeof(message));
	message* sed = (message*)sendBuffer;
	message* rec = (message*)recBuffer;

	setSyn(sed);		//设置SYN
	sed->seq_num = 0;	//设置seq=0;
	sed->ack_num = 0;	//设置ack=0
	sed->source_port = port_client;
	sed->dest_port = port_server;
	setChecksum(sed, &ph);	//设置校验和


	sendto(sockClient, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_server, len);
	clock_t start = clock(); //开始计时,如果超过限定时间就重新传输
	while (recvfrom(sockClient, recBuffer, sizeof(message), 0, (sockaddr*)&addr_server, &len) <= 0) {
		// over time
		if (clock() - start >= MAX_TIME) {
			//超时重传
			sendto(sockClient, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_server, len);
			start = clock();
		}
	}

	if (isAck(rec) && isSyn(rec) && verfiyChecksum(rec, &ph)) {
		cout << "客户端:接收报文（SYN，ACK）验证正确------第二次握手" << endl;
		memset(sendBuffer, 0, sizeof(message));
		setAck(sed);	//设置ack
		sed->seq_num = 1;
		sed->ack_num = 1;
		sed->source_port = port_client;
		sed->dest_port = port_server;
		setChecksum(sed, &ph);
		sendto(sockClient, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_server, len);
	}
	else {
		cout << "客户端:接收报文（SYN，ACK）验证错误" << endl;
		return 0;
	}

	cout << "客户端： 建立连接成功" << endl;
	return true;
}

message make_pkt(int seq, char* data, unsigned short len) {
	message message;
	memset(&message, 0, sizeof(message));
	message.source_port = port_client;
	message.dest_port = port_server;
	message.length = len;
	message.seq_num = seq;
	memcpy(message.msg, data, len);
	pseudoHead ph;		//伪首部
	memset(&ph, 0, sizeof(pseudoHead));
	ph.source_ip = inet_addr(ip_client);
	ph.dest_ip = inet_addr(ip_server);
	setChecksum(&message, &ph);	//设置校验和
	return message;
}


void rdt3_send(unsigned long length_file, char* file, char* filename) {
	//就是统计file的长度，file的名字，file就是我们传输的图片或者文本，
	int packetNUM = int(length_file / Max_Size) + (length_file % Max_Size ? 1 : 0);//最后剩余的部分不足Max_Size的也需要一个massage进行传输。
	cout << "所需要的包的数量packetNUM为 " << packetNUM << endl;

	int index = 0;
	int stage = 0;

	int len = sizeof(SOCKADDR_IN);
	int	packetDataLen = min(Max_Size, length_file - index * Max_Size);
	char* dataBuffer = new char[Max_Size];
	char* pktBuffer = new char[sizeof(message)];
	char* recpktBuffer = new char[sizeof(message)];
	message sndpkt;
	clock_t start_timer;

	message* rec = (message*)recpktBuffer;

	while (1) {
		if (index == packetNUM) { //就是最后一个报文发送的时候，我们将文件名字传输过去

			u_long imode = 1;
			ioctlsocket(sockClient, FIONBIO, &imode);//非阻塞


			char* sendBuffer = new char[sizeof(message)];
			memset(sendBuffer, 0, sizeof(message));
			message* sed = (message*)sendBuffer;
			setEnd(sed);
			sed->source_port = port_client;
			sed->dest_port = port_server;
			string fn = filename;
			int filename_len = sizeof(fn);

			memcpy(sed->msg, filename, filename_len);
			sed->length = filename_len;
			setChecksum(sed, &ph);	//设置校验和

			//发送
			sendto(sockClient, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_server, len);
			cout << "客户端： 发送报文（END）" << endl;

			clock_t start_timer = clock(); //开始计时

			while (recvfrom(sockClient, recpktBuffer, sizeof(message), 0, (sockaddr*)&addr_server, &len) <= 0 || !(isEnd(rec) && isAck(rec))) {
				// over time
				if (clock() - start_timer >= MAX_TIME) {

					//超时重传
					sendto(sockClient, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_server, len);
					cout << "客户端： 发送报文（END）,重传" << endl;
					start_timer = clock();
				}
			}


			if (isEnd(rec) && isAck(rec) && verfiyChecksum(rec, &ph)) {
				cout << "客户端：接收服务器报文（END，ACK），文件传输完成" << endl;
				return;
			}
			else
				continue;
		}


		//下面就是rdt3的传输协议，就是一共四个状态，引入了超时重传的机制。
		packetDataLen = min(Max_Size, length_file - index * Max_Size);

		switch (stage) {
		case 0:
			memcpy(dataBuffer, file + index * Max_Size, packetDataLen);
			sndpkt = make_pkt(2, dataBuffer, packetDataLen);
			memcpy(pktBuffer, &sndpkt, sizeof(message));
			sendto(sockClient, pktBuffer, sizeof(message), 0, (sockaddr*)&addr_server, len);
			start_timer = clock();
			stage = 1;
			cout << "状态0\t发送\tseq:\t2\tindex:\t" << index << "\tlength:\t" << packetDataLen << "\tchecksum:\t" << sndpkt.checksum << endl;
			break;
		case 1:
			//超时重传
			if (clock() - start_timer >= MAX_TIME) {
				sendto(sockClient, pktBuffer, sizeof(message), 0, (sockaddr*)&addr_server, len);
				cout << "状态1\t发送\tseq:\t2\tindex:\t" << index << "\tlength:\t" << packetDataLen << "\tchecksum:\t" << sndpkt.checksum << "  (重传)" << endl;
				start_timer = clock();
			}
			if (recvfrom(sockClient, recpktBuffer, sizeof(message), 0, (sockaddr*)&addr_server, &len)) {
				if (isAck(rec) && verfiyChecksum(rec, &ph) && rec->ack_num == 2) {
					cout << "状态1\t接收\tack:\t2\tAck:\t" << isAck(rec) << "\tlength:\t" << rec->length << "\tchecksum:\t" << rec->checksum << endl;
					stage = 2;
					index++;
					break;
				}
			}
			break;
		case 2:
			memcpy(dataBuffer, file + index * Max_Size, packetDataLen);
			sndpkt = make_pkt(3, dataBuffer, packetDataLen);
			memcpy(pktBuffer, &sndpkt, sizeof(message));
			sendto(sockClient, pktBuffer, sizeof(message), 0, (sockaddr*)&addr_server, len);
			cout << "状态2\t发送\tseq:\t3\tindex:\t" << index << " \tlength:\t" << packetDataLen << " \tchecksum:\t" << sndpkt.checksum << endl;
			start_timer = clock();
			stage = 3;
			break;
		case 3:
			//超时重传
			if (clock() - start_timer >= MAX_TIME) {
				sendto(sockClient, pktBuffer, sizeof(message), 0, (sockaddr*)&addr_server, len);
				cout << "状态3\t发送\tseq:\t3\tindex:\t" << index << "\tlength:\t" << packetDataLen << " \tchecksum:\t" << sndpkt.checksum << "  (重传)" << endl;
				start_timer = clock();
			}
			if (recvfrom(sockClient, recpktBuffer, sizeof(message), 0, (sockaddr*)&addr_server, &len)) {
				if (isAck(rec) && verfiyChecksum(rec, &ph) && rec->ack_num == 3) {
					cout << "状态3\t接收\tack:\t3\tAck:\t" << isAck(rec) << "\tlength:\t" << rec->length << "\tchecksum:\t" << rec->checksum << endl;
					stage = 0;
					index++;
					break;
				}
			}
			break;

		}
	}
}

bool closeConnect() {
	//四次挥手
	int len = sizeof(SOCKADDR_IN);
	char recBuffer[sizeof(message)];	//接收缓冲区
	char sendBuffer[sizeof(message)];	//发送缓冲区
	memset(recBuffer, 0, sizeof(message));
	memset(sendBuffer, 0, sizeof(message));
	message* sed = (message*)sendBuffer;
	message* rec = (message*)recBuffer;


	//（1）客户端发送FIN报文
	setFin(sed);		//设置Fin
	sed->seq_num = 0;	//设置seq=0
	sed->ack_num = 0;	//设置ack=0
	sed->source_port = port_client;
	sed->dest_port = port_server;
	setChecksum(sed, &ph);	//设置校验和
	//发送
	sendto(sockClient, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_server, len);
	cout << "客户端：发送报文（FIN）-----第一次挥手" << endl;
	clock_t start = clock(); //开始计时，超时重传

	while (recvfrom(sockClient, recBuffer, sizeof(message), 0, (sockaddr*)&addr_server, &len) <= 0) {
		// over time
		if (clock() - start >= MAX_TIME) {
			//超时重传
			sendto(sockClient, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_server, len);
			cout << "客户端：发送报文（FIN）-----第一次挥手，重传" << endl;
			start = clock();
		}
	}

	if (isAck(rec) && isFin(rec) && verfiyChecksum(rec, &ph)) {
		cout << "客户端：接收报文（FIN，ACK）验证正确----第二次挥手" << endl;
	}
	else return false;


	u_long imode = 0;
	ioctlsocket(sockClient, FIONBIO, &imode);//阻塞
	//（3）接收确定服务器端的FIN报文
	while (1) {
		recvfrom(sockClient, recBuffer, sizeof(message), 0, (sockaddr*)&addr_server, &len);
		if (isFin(rec) && verfiyChecksum(rec, &ph)) {
			cout << "客户端：接收到服务器报文（FIN）----第三次挥手，验证正确" << endl;
			break;
		}
	}


	imode = 1;
	ioctlsocket(sockClient, FIONBIO, &imode);//非阻塞
	//（4）发送FIN，ACK报文
	cleanflag(sed);
	setFin(sed);
	setAck(sed);
	setChecksum(sed, &ph);	//设置校验和
	sendto(sockClient, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_server, len);
	cout << "客户端：发送报文（FIN，ACK）---第四次挥手" << endl;

	//等待2MSL

	start = clock(); //开始计时

	while (clock() - start <= 2 * MAX_TIME) {
		//if(clock() - start>= MAX_TIME)
		if (recvfrom(sockClient, recBuffer, sizeof(message), 0, (sockaddr*)&addr_server, &len) > 0 && isFin(rec) && verfiyChecksum(rec, &ph)) {
			sendto(sockClient, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_server, len);
			cout << "客户端: 发送报文（FIN，ACK）---第四次挥手,重传" << endl;
		}
	}

	cout << "客户端：连接关闭" << endl;
	closesocket(sockClient);
	return true;
}
int main() {
	//初始化
	start();
	//建立连接
	if (!FoundConnect()) {

		cout << "建立连接失败" << endl;
	}

	cout << endl;
	cout << "/*****************************************************/" << endl;

	{
		char* filename = new char[100];
		memset(filename, 0, 100);
		string filedir;
		filedir = "send_file/1.jpg";
		memcpy(filename, "1.jpg", sizeof("1.jpg"));


		ifstream infile(filedir, ifstream::binary);

		if (!infile.is_open()) {
			cout << "无法打开文件" << endl;
			return 0;
		}

		infile.seekg(0, infile.end);
		DWORD fileLen = infile.tellg();
		infile.seekg(0, infile.beg);

		cout << "传输文件长度: " << fileLen << endl;

		char* fileBuffer = new char[fileLen];
		infile.read(fileBuffer, fileLen);
		infile.close();




		cout << "开始传输文件: " << endl;

		clock_t start_timer = clock();

		rdt3_send(fileLen, fileBuffer, filename);

		clock_t end_timer = clock();
		double endtime = (double)(end_timer - start_timer) / CLOCKS_PER_SEC;
		cout << "Total time:" << endtime << " s" << endl;
		cout << "吞吐率：" << fileLen * 8 / endtime / 1024 / 1024 << "Mbps" << endl;
	}
	{
		char* filename = new char[100];
		memset(filename, 0, 100);
		string filedir;
		filedir = "send_file/helloworld.txt";
		memcpy(filename, "helloworld.txt", sizeof("helloworld.txt"));
		ifstream infile(filedir, ifstream::binary);

		if (!infile.is_open()) {
			cout << "无法打开文件" << endl;
			return 0;
		}

		infile.seekg(0, infile.end);
		DWORD fileLen = infile.tellg();
		infile.seekg(0, infile.beg);

		cout << "传输文件长度: " << fileLen << endl;

		char* fileBuffer = new char[fileLen];
		infile.read(fileBuffer, fileLen);
		infile.close();

		cout << "开始传输文件: " << endl;
		clock_t start_timer = clock();
		rdt3_send(fileLen, fileBuffer, filename);
		clock_t end_timer = clock();
		double endtime = (double)(end_timer - start_timer) / CLOCKS_PER_SEC;
		cout << "Total time:" << endtime << " s" << endl;
		cout << "吞吐率：" << fileLen * 8 / endtime / 1024 / 1024 << "Mbps" << endl;

	}
	closeConnect();
	WSACleanup();
	system("pause");
}