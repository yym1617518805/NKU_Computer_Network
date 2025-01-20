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
#define  Max_Size 8192  //ÿһ�δ�����ļ����ֽ���
#define MAX_TIME 1000  //�೤ʱ��֮��������Ҫ�ش�
using namespace std;

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned long DWORD;

struct message{
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
bool isAck(message*message) {
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


SOCKET sockServer;
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

	addr_client.sin_port = htons(port_route);	//���ö˿ںţ����������ֽ�������תΪ������֮���ٽ��з�ת
	addr_client.sin_addr.S_un.S_addr = inet_addr(ip_route); //�ʺϴ���Ķ�����IP��ʽ
	addr_client.sin_family = AF_INET; //��ʾ����������Э�飬��ʾIPV4��ַ��
	//����ķ���˵Ĳ���Ҳ���������á�
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

	//��ʼ��α�ײ�
	memset(&ph, 0, sizeof(pseudoHead));
	ph.source_ip = inet_addr(ip_client);
	ph.dest_ip = inet_addr(ip_server);

	cout << "��ʼ���ɹ����������˿�ʼ����������������" << endl;
}

bool Found_Connect() {
	int len = sizeof(SOCKADDR_IN);
	char recBuffer[sizeof(message)];	//���ջ�����
	char sendBuffer[sizeof(message)];	//���ͻ�����
	message* sed = (message*)sendBuffer;
	message* rec = (message*)recBuffer;
	memset(recBuffer, 0, sizeof(message));
	memset(sendBuffer, 0, sizeof(message));//��ʼ��������Ϣ�ͷ�����Ϣ�Ĳ�����


	while (1) {
		//����������SYN,���û�����Կͻ��˵���Ϣ���ͣ�ֱ��ͣ��
		recvfrom(sockServer, recBuffer, sizeof(message), 0, (sockaddr*)&addr_client, &len);
		if (isSyn(rec) && verfiyChecksum(rec, &ph) && rec->seq_num == 0) {
			cout << "�������ˣ� ���յ��ͻ���SYN���ģ���֤�ɹ�----��һ������" << endl;
			//����SYN��ACK����
			setAck(sed);
			setSyn(sed);
			sed->seq_num = 0;
			sed->ack_num = 1;
			sed->source_port = port_server;
			sed->dest_port = port_client;
			setChecksum(sed, &ph);
			//����SYN��ACK
			sendto(sockServer, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_client, len);
			break;
		}
		else {
			cout << "�������ˣ� ���յ��ͻ���SYN���ģ���֤ʧ��" << endl;
			continue;
		}
	}


	u_long imode = 1;
	ioctlsocket(sockServer, FIONBIO, &imode);//������
	clock_t start = clock(); //��ʼ��ʱ
	while (recvfrom(sockServer, recBuffer, sizeof(message), 0, (sockaddr*)&addr_client, &len) <= 0) {
		// over time
		if (clock() - start >= MAX_TIME) {
			//��ʱ�ش�
			sendto(sockServer, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_client, len);
			cout << "�������ˣ� �ش����ģ�SYN��ACK��" << endl;
			start = clock();
		}
	}
	if (isAck(rec) && verfiyChecksum(rec, &ph)) {
		cout << "�������ˣ����յ��ͻ��˱��ģ�SYN��ACK����֤��ȷ-----����������" << endl;
	}
	else {
		return false;
	}
	imode = 0;
	ioctlsocket(sockServer, FIONBIO, &imode);//����
	return true;
}


//������Ϣ������
message make_pkt(int ack) {
	message messages;
	memset(&messages, 0, sizeof(message));
	messages.source_port = port_client;
	messages.dest_port = port_server;
	setAck(&messages);
	messages.ack_num = ack;
	pseudoHead ph;		//α�ײ�
	memset(&ph, 0, sizeof(pseudoHead));
	ph.source_ip = inet_addr(ip_client);
	ph.dest_ip = inet_addr(ip_server);
	setChecksum(&messages, &ph);	//����У���
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
				cout << "�������" << endl;
				memset(sendBuffer, 0, sizeof(message));
				message sed = make_pkt(0);
				setEnd(&sed);
				sed.checksum = 0;
				setChecksum(&sed, &ph);
				memcpy(sendBuffer, &sed, sizeof(message));
				sendto(sockServer, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_client, len);
				cout << "������: ���ͱ��ģ�END��ACK��" << endl;
				return rec_data_len;
			}
			if (rec->seq_num == 3 || !(verfiyChecksum(rec, &ph))) {
				message sedpkt = make_pkt(3);
				memcpy(sendBuffer, &sedpkt, sizeof(message));
				sendto(sockServer, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_client, len);
				cout << "״̬0\t����\tseq:\t3\t" << endl;
				cout << "״̬0\t����\tack:\t3\tACK:\t" << isAck(&sedpkt) << "\tlength:\t" << sedpkt.length << "\tchecksum:\t" << sedpkt.checksum << endl;
				stage = 0;
				break;

			}
			//��ȷ����
			if (rec->seq_num == 2 && (verfiyChecksum(rec, &ph))) {
				message sedpkt = make_pkt(2);
				memcpy(sendBuffer, &sedpkt, sizeof(message));
				sendto(sockServer, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_client, len);
				cout << "״̬0\t����\tseq:\t2\tindex:\t" << index << "\tlength:\t" << rec->length << "\tchecksum:\t" << rec->checksum << endl;
				cout << "״̬0\t����\tack:\t2\tACK:\t" << isAck(&sedpkt) << "\tlength:\t" << sedpkt.length << "\tchecksum:\t" << sedpkt.checksum << endl;
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
				cout << "�������" << endl;
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
				cout << "״̬1\t����\tseq:\t2\t" << endl;
				cout << "״̬1\t����\tack:\t2\tACK:\t" << isAck(&sedpkt) << "\tlength:\t" << sedpkt.length << "\tchecksum:\t" << sedpkt.checksum << endl;
				stage = 1;
				break;
			}
			if (rec->seq_num == 3 && (verfiyChecksum(rec, &ph))) {
				message sedpkt = make_pkt(3);
				memcpy(sendBuffer, &sedpkt, sizeof(message));
				sendto(sockServer, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_client, len);
				cout << "״̬1\t����\tseq:\t3\tindex:\t" << index << "\tlength:\t" << rec->length << "\tchecksum:\t" << rec->checksum << endl;
				cout << "״̬1\t����\tack:\t3\tACK:\t" << isAck(&sedpkt) << "\tlength:\t" << sedpkt.length << "\tchecksum:\t" << sedpkt.checksum << endl;
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
	char recBuffer[sizeof(message)];	//���ջ�����
	char sendBuffer[sizeof(message)];	//���ͻ�����
	memset(recBuffer, 0, sizeof(message));
	memset(sendBuffer, 0, sizeof(message));
	message* sed = (message*)sendBuffer;
	message* rec = (message*)recBuffer;

	u_long imode = 0;
	ioctlsocket(sockServer, FIONBIO, &imode);//����

	while (1) {
		recvfrom(sockServer, recBuffer, sizeof(message), 0, (sockaddr*)&addr_server, &len);

		if (isFin(rec) && verfiyChecksum(rec, &ph)) {

			cout << "�������� �յ��ͻ���Fin������֤��ȷ----��һ�λ���" << endl;
			break;
		}
	}


	//�ڶ��Σ����������÷���ACK��FIN����
	setFin(sed);		//����Fin
	setAck(sed);		//����Ack
	sed->seq_num = 0;	//����seq=0
	sed->ack_num = 0;	//����ack=0
	sed->source_port = port_client;
	sed->dest_port = port_server;
	setChecksum(sed, &ph);	//����У���
	//����
	sendto(sockServer, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_client, len);
	cout << "�������� ����ACK,Fin����----�ڶ��λ���" << endl;

	//����FIN
	cleanflag(sed);
	setFin(sed);		//����Fin
	setChecksum(sed, &ph);
	sendto(sockServer, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_client, len);
	cout << "�������� ����Fin����ʣ�����ݣ�----�����λ��֣���֤��ȷ" << endl;

	imode = 1;
	ioctlsocket(sockServer, FIONBIO, &imode);//������

	//����
	clock_t  start = clock(); //��ʼ��ʱ
	while (recvfrom(sockServer, recBuffer, sizeof(message), 0, (sockaddr*)&addr_server, &len) <= 0 || !(isAck(rec) && isFin(rec) && verfiyChecksum(rec, &ph))) {
		// over time
		if (clock() - start >= MAX_TIME) {
			//��ʱ�ش�
			sendto(sockServer, sendBuffer, sizeof(message), 0, (sockaddr*)&addr_client, len);
			cout << "�������� ���ͱ��ģ�FIN��----�����λ��֣��ش�" << endl;
			start = clock();
		}
	}

	if (isAck(rec) && isFin(rec) && verfiyChecksum(rec, &ph)) {
		cout << "�������� ���յ����ģ�FIN��ACK������֤��ȷ---���Ĵλ���" << endl;
	}
	else return false;

	cout << "���ӹر�" << endl;
	closesocket(sockServer);
	return true;
}
int main() {

	start();
	if (Found_Connect()) {
		cout << "����ˣ� �������ӳɹ�" << endl;
	}

	cout << endl;
	cout << "/*****************************************************/" << endl;

	//�����ļ��Ļ�����

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


		//д�븴���ļ�
		ofstream outfile(filenm, ios::binary);

		outfile.write(fileBuffer, fileLength);
		outfile.close();

		cout << "�Ƿ�������ܴ��䣨Y/N���� ";

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