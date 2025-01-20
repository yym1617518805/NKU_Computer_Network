#include<fstream>
#include<iostream>
#include <iostream>
#include<WinSock2.h>
#include<time.h>
#include<stdio.h>
#include<cmath>
#include<stdio.h>
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

struct message{
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
bool isAck(message*message) {
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


SOCKET sockServer;
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

	addr_client.sin_port = htons(port_route);	//设置端口号，就是网络字节序大端序。转为二进制之后再进行翻转
	addr_client.sin_addr.S_un.S_addr = inet_addr(ip_route); //适合传输的二进制IP形式
	addr_client.sin_family = AF_INET; //表示是那种网络协议，表示IPV4地址族
	//下面的服务端的操作也是这样设置。
	addr_server.sin_port = htons(port_server);
	addr_server.sin_addr.S_un.S_addr = inet_addr(ip_server);
	addr_server.sin_family = AF_INET;

	sockServer = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockServer == INVALID_SOCKET) {
		cout << "socket creat failed" << endl;
		return;
	}

	//bind
	int result_bind;
	result_bind = bind(sockServer, (SOCKADDR*)&addr_server, sizeof(SOCKADDR_IN));
	if (result_bind == SOCKET_ERROR) {
		cout << "bind failed" << endl;
		return;
	}

	//初始化伪首部
	memset(&ph, 0, sizeof(pseudoHead));
	ph.source_ip = inet_addr(ip_client);
	ph.dest_ip = inet_addr(ip_server);

	cout << "初始化成功，服务器端开始进行三次握手链接" << endl;
}

bool Found_Connect() {
	int len = sizeof(SOCKADDR_IN);
	char recBuffer[sizeof(message)];	//接收缓冲区
	char sendBuffer[sizeof(message)];	//发送缓冲区
	message* sed = (message*)sendBuffer;
	message* rec = (message*)recBuffer;
	memset(recBuffer, 0, sizeof(message));
	memset(sendBuffer, 0, sizeof(message));//初始化接收信息和发送信息的操作。


	while (1) {
		//阻塞，接收SYN,如果没有来自客户端的消息发送，直接停滞
		recvfrom(sockServer, recBuffer, sizeof(message), 0, (sockaddr*)&addr_client, &len);
		if (isSyn(rec) && verfiyChecksum(rec, &ph) && rec->seq_num == 0) {
			cout << "服务器端： 接收到客户端SYN报文，验证成功----第一次握手" << endl;
			//设置SYN，ACK报文
			setAck(sed);
			setSyn(sed);
			sed->seq_num = 0;
			sed->ack_num = 1;
			sed->source_port = port_server;
			sed->dest_port = port_client;
			setChecksum(sed, &ph);
			//发送SYN，ACK
			sendto(sockServer, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_client, len);
			break;
		}
		else {
			cout << "服务器端： 接收到客户端SYN报文，验证失败" << endl;
			continue;
		}
	}


	u_long imode = 1;
	ioctlsocket(sockServer, FIONBIO, &imode);//非阻塞
	clock_t start = clock(); //开始计时
	while (recvfrom(sockServer, recBuffer, sizeof(message), 0, (sockaddr*)&addr_client, &len) <= 0) {
		// over time
		if (clock() - start >= MAX_TIME) {
			//超时重传
			sendto(sockServer, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_client, len);
			cout << "服务器端： 重传报文（SYN，ACK）" << endl;
			start = clock();
		}
	}
	if (isAck(rec) && verfiyChecksum(rec, &ph)) {
		cout << "服务器端：接收到客户端报文（SYN，ACK）验证正确-----第三次握手" << endl;
	}
	else {
		return false;
	}
	imode = 0;
	ioctlsocket(sockServer, FIONBIO, &imode);//阻塞
	return true;
}


//设置消息的属性
message make_pkt(int ack) {
	message messages;
	memset(&messages, 0, sizeof(message));
	messages.source_port = port_client;
	messages.dest_port = port_server;
	setAck(&messages);
	messages.ack_num = ack;
	pseudoHead ph;		//伪首部
	memset(&ph, 0, sizeof(pseudoHead));
	ph.source_ip = inet_addr(ip_client);
	ph.dest_ip = inet_addr(ip_server);
	setChecksum(&messages, &ph);	//设置校验和
	return messages;
}


DWORD rdt3_receive(char* file, char* filename) {
	int index = 0;
	int stage = 0;
	int len = sizeof(SOCKADDR_IN);
	char* pktBuffer = new char[sizeof(message)];
	char* recpktBuffer = new char[sizeof(message)];
	char* sendBuffer = new char[sizeof(message)];

	DWORD rec_data_len = 0;
	message* rec = (message*)recpktBuffer;

	u_long imode = 0;
	if (ioctlsocket(sockServer, FIONBIO, &imode) == SOCKET_ERROR)
		cout << "error" << endl;
	bool start_tran = 0;

	while (1) {
		memset(recpktBuffer, 0, sizeof(message));

		switch (stage) {
		case 0:
			if (recvfrom(sockServer, recpktBuffer, sizeof(message), 0, (sockaddr*)&addr_client, &len) > 0 && rec->length != 0) {}
			else break;
			if (isEnd(rec) && start_tran) {
				memcpy(filename, rec->msg, rec->length);
				cout << "传输完毕" << endl;
				memset(sendBuffer, 0, sizeof(message));
				message sed = make_pkt(0);
				setEnd(&sed);
				sed.checksum = 0;
				setChecksum(&sed, &ph);
				memcpy(sendBuffer, &sed, sizeof(message));
				sendto(sockServer, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_client, len);
				cout << "服务器: 发送报文（END，ACK）" << endl;
				return rec_data_len;
			}
			if (rec->seq_num == 3 || !(verfiyChecksum(rec, &ph))) {
				message sedpkt = make_pkt(3);
				memcpy(sendBuffer, &sedpkt, sizeof(message));
				sendto(sockServer, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_client, len);
				cout << "状态0\t接收\tseq:\t3\t" << endl;
				cout << "状态0\t发送\tack:\t3\tACK:\t" << isAck(&sedpkt) << "\tlength:\t" << sedpkt.length << "\tchecksum:\t" << sedpkt.checksum << endl;
				stage = 0;
				break;

			}
			//正确接收
			if (rec->seq_num == 2 && (verfiyChecksum(rec, &ph))) {
				message sedpkt = make_pkt(2);
				memcpy(sendBuffer, &sedpkt, sizeof(message));
				sendto(sockServer, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_client, len);
				cout << "状态0\t接收\tseq:\t2\tindex:\t" << index << "\tlength:\t" << rec->length << "\tchecksum:\t" << rec->checksum << endl;
				cout << "状态0\t发送\tack:\t2\tACK:\t" << isAck(&sedpkt) << "\tlength:\t" << sedpkt.length << "\tchecksum:\t" << sedpkt.checksum << endl;
				memcpy(file + rec_data_len, rec->msg, rec->length);
				rec_data_len += rec->length;
				stage = 1;
				start_tran = 1;
				index++;
				break;
			}
			break;


		case 1:
			recvfrom(sockServer, recpktBuffer, sizeof(message), 0, (sockaddr*)&addr_client, &len);

			if (isEnd(rec) && start_tran) {
				memcpy(filename, rec->msg, rec->length);
				cout << "传输完毕" << endl;
				memset(sendBuffer, 0, sizeof(message));
				message sed = make_pkt(0);
				setEnd(&sed);
				sed.checksum = 0;
				setChecksum(&sed, &ph);
				memcpy(sendBuffer, &sed, sizeof(message));
				sendto(sockServer, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_client, len);
				return rec_data_len;
			}


			if (rec->seq_num == 2 || !(verfiyChecksum(rec, &ph))) {
				message sedpkt = make_pkt(2);
				memcpy(sendBuffer, &sedpkt, sizeof(message));
				sendto(sockServer, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_client, len);
				cout << "状态1\t接收\tseq:\t2\t" << endl;
				cout << "状态1\t发送\tack:\t2\tACK:\t" << isAck(&sedpkt) << "\tlength:\t" << sedpkt.length << "\tchecksum:\t" << sedpkt.checksum << endl;
				stage = 1;
				break;
			}
			if (rec->seq_num == 3 && (verfiyChecksum(rec, &ph))) {
				message sedpkt = make_pkt(3);
				memcpy(sendBuffer, &sedpkt, sizeof(message));
				sendto(sockServer, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_client, len);
				cout << "状态1\t接收\tseq:\t3\tindex:\t" << index << "\tlength:\t" << rec->length << "\tchecksum:\t" << rec->checksum << endl;
				cout << "状态1\t发送\tack:\t3\tACK:\t" << isAck(&sedpkt) << "\tlength:\t" << sedpkt.length << "\tchecksum:\t" << sedpkt.checksum << endl;
				memcpy(file + rec_data_len, rec->msg, rec->length);
				rec_data_len += rec->length;
				start_tran = 1;
				stage = 0;
				index++;
				break;
			}
			break;
		}
	}
}

bool closeConnect() {
	int len = sizeof(SOCKADDR_IN);
	char recBuffer[sizeof(message)];	//接收缓冲区
	char sendBuffer[sizeof(message)];	//发送缓冲区
	memset(recBuffer, 0, sizeof(message));
	memset(sendBuffer, 0, sizeof(message));
	message* sed = (message*)sendBuffer;
	message* rec = (message*)recBuffer;

	u_long imode = 0;
	ioctlsocket(sockServer, FIONBIO, &imode);//阻塞

	while (1) {
		recvfrom(sockServer, recBuffer, sizeof(message), 0, (sockaddr*)&addr_server, &len);

		if (isFin(rec) && verfiyChecksum(rec, &ph)) {

			cout << "服务器： 收到客户端Fin请求，验证正确----第一次挥手" << endl;
			break;
		}
	}


	//第二次：服务器设置发送ACK，FIN报文
	setFin(sed);		//设置Fin
	setAck(sed);		//设置Ack
	sed->seq_num = 0;	//设置seq=0
	sed->ack_num = 0;	//设置ack=0
	sed->source_port = port_client;
	sed->dest_port = port_server;
	setChecksum(sed, &ph);	//设置校验和
	//发送
	sendto(sockServer, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_client, len);
	cout << "服务器： 发送ACK,Fin请求----第二次挥手" << endl;

	//发送FIN
	cleanflag(sed);
	setFin(sed);		//设置Fin
	setChecksum(sed, &ph);
	sendto(sockServer, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_client, len);
	cout << "服务器： 发送Fin请求（剩余数据）----第三次挥手，验证正确" << endl;

	imode = 1;
	ioctlsocket(sockServer, FIONBIO, &imode);//非阻塞

	//接收
	clock_t  start = clock(); //开始计时
	while (recvfrom(sockServer, recBuffer, sizeof(message), 0, (sockaddr*)&addr_server, &len) <= 0 || !(isAck(rec) && isFin(rec) && verfiyChecksum(rec, &ph))) {
		// over time
		if (clock() - start >= MAX_TIME) {
			//超时重传
			sendto(sockServer, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_client, len);
			cout << "服务器： 发送报文（FIN）----第三次挥手，重传" << endl;
			start = clock();
		}
	}

	if (isAck(rec) && isFin(rec) && verfiyChecksum(rec, &ph)) {
		cout << "服务器： 接收到报文（FIN，ACK），验证正确---第四次挥手" << endl;
	}
	else return false;

	cout << "连接关闭" << endl;
	closesocket(sockServer);
	return true;
}
int main() {

	start();
	if (Found_Connect()) {
		cout << "服务端： 建立连接成功" << endl;
	}

	cout << endl;
	cout << "/*****************************************************/" << endl;

	//接收文件的缓冲区

	bool tran = 1;

	while (tran) {
		char* fileBuffer = new char[90000000];
		DWORD fileLength = 0;
		char* filename = new char[100];
		memset(filename, 0, 100);


		fileLength = rdt3_receive(fileBuffer, filename);

		cout << "/*****************************************************/" << endl;
		cout << endl;
		//string dir = "C:/Users/nan/Desktop/lab3/lab3_1/rec file/";
		string dir = "receive_file/";
		string  fn = filename;
		string filenm = dir + fn;


		//写入复制文件
		ofstream outfile(filenm, ios::binary);

		outfile.write(fileBuffer, fileLength);
		outfile.close();

		cout << "是否继续接受传输（Y/N）： ";

		char i;
		cin >> i;

		cout << "/*****************************************************/" << endl;
		cout << endl;
		switch (i)
		{
		case 'y':
			tran = 1;
			break;
		case 'n':
			tran = 0;
			break;

		default:
			break;
		}

	}
	closeConnect();
	WSACleanup();
	system("pause");
}