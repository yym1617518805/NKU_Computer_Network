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
#define  Max_Size 8192  //ÿһ�δ�����ļ����ֽ���
#define MAX_TIME 1000  //�೤ʱ��֮��������Ҫ�ش�
using namespace std;

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned long DWORD;

struct message {
	WORD source_port = 0, dest_port = 0;	//port
	DWORD seq_num = 0, ack_num = 0;			//seq,ack
	WORD  length = 0;					//length
	BYTE   flag = 0;				     //flag
	WORD checksum = 0;					//У���
	char msg[Max_Size] = { 0 };
};//����

struct pseudoHead {
	DWORD source_ip = 0, dest_ip = 0;		//ip
	char zero = 0;
	char protocol = 0;
	WORD length = sizeof(message);
}; //α�ײ�

//�����������message�Ľṹ�����Ϣ
void setlength(message* message, short int len) {
	message->length = len & 0x0004;
}
short int getlength(message& message) {
	return message.length;
}

//ACK=0x01, SYN=0x02, FIN=0x04, END=0x08 
//����״̬��END�ں�����ʾ���һ�����Ĵ��䡣
void cleanflag(message* message) {
	message->flag &= 0x0;
}//��ձ�־λ
void setAck(message* message) {
	message->flag |= 0x01;
}//����־λ����ΪACK����ʾȷ�ϡ�
bool isAck(message* message) {
	return message->flag & 0x01;
}//�ж��ǲ���Ack�����գ���ģʽ
void setSyn(message* message) {
	message->flag |= 0x02;
}//����flag��־λΪ��������
bool isSyn(message* message) {
	return message->flag & 0x02;
}//�ǲ��ǽ������ӵ�״̬��
void setFin(message* message) {
	message->flag |= 0x04;
}//��flag����Ϊ�ر�����
bool isFin(message* message) {
	return message->flag & 0x04;
}//�жϷ��͵���Ϣ�ǲ�����Ҫ�ر�����
void setEnd(message* message) {
	message->flag |= 0x08;
}//�ڱ�׼�Ĺ淶����ӵ�״̬����ʾ�ǲ������һ�����ݰ��Ĵ���
bool isEnd(message* message) {
	return message->flag & 0x08;
}//�ж��ǲ������һ����
void setMsg(message* message, char* data) {
	memcpy(message->msg, data, Max_Size);
}//���ǽ����ǵ�ǰ��ȡ�����ݽ��п���;
void setChecksum(message* messages, pseudoHead* ph) {
	//��Ϊ0
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
	//���ǽ����е����ݰ�WORD(ʮ��λ)��ӣ�����ʮ��λ������ƽ�Ƶ�ĩβ�ټ��ϡ�
	//���Խ��ȡ���ͳ�ΪУ��λ��
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
	//��ͬ�Ĳ����������sum��ͬʱ��ȡ������ô���Ǻ�ȡ���Ľ����ӣ��϶�ʮ��λ����ȫ��Ϊ1.
	return sum == 0xffff;
};


SOCKET sockClient;
SOCKADDR_IN addr_client;
SOCKADDR_IN addr_server;


//������Ƕ���ip�Լ��˿ںŵĳ�ʼ��
char  ip_client[] = "127.0.0.1";
WORD  port_client = 8079;
char ip_server[] = "127.0.0.1";
WORD  port_server = 8080;
char ip_route[] = "127.0.0.1";
WORD  port_route = 8081;

pseudoHead ph;		//α�ײ�

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
	ioctlsocket(sockClient, FIONBIO, &imode);//������

	//bind
	int result_bind;
	result_bind = bind(sockClient, (SOCKADDR*)&addr_client, sizeof(SOCKADDR_IN));
	if (result_bind == SOCKET_ERROR) {
		cout << "bind failed" << endl;
		return;
	}
	//��ʼ��αͷ��
	memset(&ph, 0, sizeof(pseudoHead));
	ph.source_ip = inet_addr(ip_client);
	ph.dest_ip = inet_addr(ip_server);
	cout << "��ʼ���ɹ����ͻ��˿�ʼ����������������" << endl;
}

bool FoundConnect() {

	int len = sizeof(SOCKADDR_IN);
	char recBuffer[sizeof(message)];	//���ջ�����
	char sendBuffer[sizeof(message)];	//���ͻ�����
	memset(recBuffer, 0, sizeof(message));
	memset(sendBuffer, 0, sizeof(message));
	message* sed = (message*)sendBuffer;
	message* rec = (message*)recBuffer;

	setSyn(sed);		//����SYN
	sed->seq_num = 0;	//����seq=0;
	sed->ack_num = 0;	//����ack=0
	sed->source_port = port_client;
	sed->dest_port = port_server;
	setChecksum(sed, &ph);	//����У���


	sendto(sockClient, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_server, len);
	clock_t start = clock(); //��ʼ��ʱ,��������޶�ʱ������´���
	while (recvfrom(sockClient, recBuffer, sizeof(message), 0, (sockaddr*)&addr_server, &len) <= 0) {
		// over time
		if (clock() - start >= MAX_TIME) {
			//��ʱ�ش�
			sendto(sockClient, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_server, len);
			start = clock();
		}
	}

	if (isAck(rec) && isSyn(rec) && verfiyChecksum(rec, &ph)) {
		cout << "�ͻ���:���ձ��ģ�SYN��ACK����֤��ȷ------�ڶ�������" << endl;
		memset(sendBuffer, 0, sizeof(message));
		setAck(sed);	//����ack
		sed->seq_num = 1;
		sed->ack_num = 1;
		sed->source_port = port_client;
		sed->dest_port = port_server;
		setChecksum(sed, &ph);
		sendto(sockClient, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_server, len);
	}
	else {
		cout << "�ͻ���:���ձ��ģ�SYN��ACK����֤����" << endl;
		return 0;
	}

	cout << "�ͻ��ˣ� �������ӳɹ�" << endl;
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
	pseudoHead ph;		//α�ײ�
	memset(&ph, 0, sizeof(pseudoHead));
	ph.source_ip = inet_addr(ip_client);
	ph.dest_ip = inet_addr(ip_server);
	setChecksum(&message, &ph);	//����У���
	return message;
}


void rdt3_send(unsigned long length_file, char* file, char* filename) {
	//����ͳ��file�ĳ��ȣ�file�����֣�file�������Ǵ����ͼƬ�����ı���
	int packetNUM = int(length_file / Max_Size) + (length_file % Max_Size ? 1 : 0);//���ʣ��Ĳ��ֲ���Max_Size��Ҳ��Ҫһ��massage���д��䡣
	cout << "����Ҫ�İ�������packetNUMΪ " << packetNUM << endl;

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
		if (index == packetNUM) { //�������һ�����ķ��͵�ʱ�����ǽ��ļ����ִ����ȥ

			u_long imode = 1;
			ioctlsocket(sockClient, FIONBIO, &imode);//������


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
			setChecksum(sed, &ph);	//����У���

			//����
			sendto(sockClient, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_server, len);
			cout << "�ͻ��ˣ� ���ͱ��ģ�END��" << endl;

			clock_t start_timer = clock(); //��ʼ��ʱ

			while (recvfrom(sockClient, recpktBuffer, sizeof(message), 0, (sockaddr*)&addr_server, &len) <= 0 || !(isEnd(rec) && isAck(rec))) {
				// over time
				if (clock() - start_timer >= MAX_TIME) {

					//��ʱ�ش�
					sendto(sockClient, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_server, len);
					cout << "�ͻ��ˣ� ���ͱ��ģ�END��,�ش�" << endl;
					start_timer = clock();
				}
			}


			if (isEnd(rec) && isAck(rec) && verfiyChecksum(rec, &ph)) {
				cout << "�ͻ��ˣ����շ��������ģ�END��ACK�����ļ��������" << endl;
				return;
			}
			else
				continue;
		}


		//�������rdt3�Ĵ���Э�飬����һ���ĸ�״̬�������˳�ʱ�ش��Ļ��ơ�
		packetDataLen = min(Max_Size, length_file - index * Max_Size);

		switch (stage) {
		case 0:
			memcpy(dataBuffer, file + index * Max_Size, packetDataLen);
			sndpkt = make_pkt(2, dataBuffer, packetDataLen);
			memcpy(pktBuffer, &sndpkt, sizeof(message));
			sendto(sockClient, pktBuffer, sizeof(message), 0, (sockaddr*)&addr_server, len);
			start_timer = clock();
			stage = 1;
			cout << "״̬0\t����\tseq:\t2\tindex:\t" << index << "\tlength:\t" << packetDataLen << "\tchecksum:\t" << sndpkt.checksum << endl;
			break;
		case 1:
			//��ʱ�ش�
			if (clock() - start_timer >= MAX_TIME) {
				sendto(sockClient, pktBuffer, sizeof(message), 0, (sockaddr*)&addr_server, len);
				cout << "״̬1\t����\tseq:\t2\tindex:\t" << index << "\tlength:\t" << packetDataLen << "\tchecksum:\t" << sndpkt.checksum << "  (�ش�)" << endl;
				start_timer = clock();
			}
			if (recvfrom(sockClient, recpktBuffer, sizeof(message), 0, (sockaddr*)&addr_server, &len)) {
				if (isAck(rec) && verfiyChecksum(rec, &ph) && rec->ack_num == 2) {
					cout << "״̬1\t����\tack:\t2\tAck:\t" << isAck(rec) << "\tlength:\t" << rec->length << "\tchecksum:\t" << rec->checksum << endl;
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
			cout << "״̬2\t����\tseq:\t3\tindex:\t" << index << " \tlength:\t" << packetDataLen << " \tchecksum:\t" << sndpkt.checksum << endl;
			start_timer = clock();
			stage = 3;
			break;
		case 3:
			//��ʱ�ش�
			if (clock() - start_timer >= MAX_TIME) {
				sendto(sockClient, pktBuffer, sizeof(message), 0, (sockaddr*)&addr_server, len);
				cout << "״̬3\t����\tseq:\t3\tindex:\t" << index << "\tlength:\t" << packetDataLen << " \tchecksum:\t" << sndpkt.checksum << "  (�ش�)" << endl;
				start_timer = clock();
			}
			if (recvfrom(sockClient, recpktBuffer, sizeof(message), 0, (sockaddr*)&addr_server, &len)) {
				if (isAck(rec) && verfiyChecksum(rec, &ph) && rec->ack_num == 3) {
					cout << "״̬3\t����\tack:\t3\tAck:\t" << isAck(rec) << "\tlength:\t" << rec->length << "\tchecksum:\t" << rec->checksum << endl;
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
	//�Ĵλ���
	int len = sizeof(SOCKADDR_IN);
	char recBuffer[sizeof(message)];	//���ջ�����
	char sendBuffer[sizeof(message)];	//���ͻ�����
	memset(recBuffer, 0, sizeof(message));
	memset(sendBuffer, 0, sizeof(message));
	message* sed = (message*)sendBuffer;
	message* rec = (message*)recBuffer;


	//��1���ͻ��˷���FIN����
	setFin(sed);		//����Fin
	sed->seq_num = 0;	//����seq=0
	sed->ack_num = 0;	//����ack=0
	sed->source_port = port_client;
	sed->dest_port = port_server;
	setChecksum(sed, &ph);	//����У���
	//����
	sendto(sockClient, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_server, len);
	cout << "�ͻ��ˣ����ͱ��ģ�FIN��-----��һ�λ���" << endl;
	clock_t start = clock(); //��ʼ��ʱ����ʱ�ش�

	while (recvfrom(sockClient, recBuffer, sizeof(message), 0, (sockaddr*)&addr_server, &len) <= 0) {
		// over time
		if (clock() - start >= MAX_TIME) {
			//��ʱ�ش�
			sendto(sockClient, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_server, len);
			cout << "�ͻ��ˣ����ͱ��ģ�FIN��-----��һ�λ��֣��ش�" << endl;
			start = clock();
		}
	}

	if (isAck(rec) && isFin(rec) && verfiyChecksum(rec, &ph)) {
		cout << "�ͻ��ˣ����ձ��ģ�FIN��ACK����֤��ȷ----�ڶ��λ���" << endl;
	}
	else return false;


	u_long imode = 0;
	ioctlsocket(sockClient, FIONBIO, &imode);//����
	//��3������ȷ���������˵�FIN����
	while (1) {
		recvfrom(sockClient, recBuffer, sizeof(message), 0, (sockaddr*)&addr_server, &len);
		if (isFin(rec) && verfiyChecksum(rec, &ph)) {
			cout << "�ͻ��ˣ����յ����������ģ�FIN��----�����λ��֣���֤��ȷ" << endl;
			break;
		}
	}


	imode = 1;
	ioctlsocket(sockClient, FIONBIO, &imode);//������
	//��4������FIN��ACK����
	cleanflag(sed);
	setFin(sed);
	setAck(sed);
	setChecksum(sed, &ph);	//����У���
	sendto(sockClient, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_server, len);
	cout << "�ͻ��ˣ����ͱ��ģ�FIN��ACK��---���Ĵλ���" << endl;

	//�ȴ�2MSL

	start = clock(); //��ʼ��ʱ

	while (clock() - start <= 2 * MAX_TIME) {
		//if(clock() - start>= MAX_TIME)
		if (recvfrom(sockClient, recBuffer, sizeof(message), 0, (sockaddr*)&addr_server, &len) > 0 && isFin(rec) && verfiyChecksum(rec, &ph)) {
			sendto(sockClient, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_server, len);
			cout << "�ͻ���: ���ͱ��ģ�FIN��ACK��---���Ĵλ���,�ش�" << endl;
		}
	}

	cout << "�ͻ��ˣ����ӹر�" << endl;
	closesocket(sockClient);
	return true;
}
int main() {
	//��ʼ��
	start();
	//��������
	if (!FoundConnect()) {

		cout << "��������ʧ��" << endl;
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
			cout << "�޷����ļ�" << endl;
			return 0;
		}

		infile.seekg(0, infile.end);
		DWORD fileLen = infile.tellg();
		infile.seekg(0, infile.beg);

		cout << "�����ļ�����: " << fileLen << endl;

		char* fileBuffer = new char[fileLen];
		infile.read(fileBuffer, fileLen);
		infile.close();




		cout << "��ʼ�����ļ�: " << endl;

		clock_t start_timer = clock();

		rdt3_send(fileLen, fileBuffer, filename);

		clock_t end_timer = clock();
		double endtime = (double)(end_timer - start_timer) / CLOCKS_PER_SEC;
		cout << "Total time:" << endtime << " s" << endl;
		cout << "�����ʣ�" << fileLen * 8 / endtime / 1024 / 1024 << "Mbps" << endl;
	}
	{
		char* filename = new char[100];
		memset(filename, 0, 100);
		string filedir;
		filedir = "send_file/helloworld.txt";
		memcpy(filename, "helloworld.txt", sizeof("helloworld.txt"));
		ifstream infile(filedir, ifstream::binary);

		if (!infile.is_open()) {
			cout << "�޷����ļ�" << endl;
			return 0;
		}

		infile.seekg(0, infile.end);
		DWORD fileLen = infile.tellg();
		infile.seekg(0, infile.beg);

		cout << "�����ļ�����: " << fileLen << endl;

		char* fileBuffer = new char[fileLen];
		infile.read(fileBuffer, fileLen);
		infile.close();

		cout << "��ʼ�����ļ�: " << endl;
		clock_t start_timer = clock();
		rdt3_send(fileLen, fileBuffer, filename);
		clock_t end_timer = clock();
		double endtime = (double)(end_timer - start_timer) / CLOCKS_PER_SEC;
		cout << "Total time:" << endtime << " s" << endl;
		cout << "�����ʣ�" << fileLen * 8 / endtime / 1024 / 1024 << "Mbps" << endl;

	}
	closeConnect();
	WSACleanup();
	system("pause");
}