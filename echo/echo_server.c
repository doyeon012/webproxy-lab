#include "csapp.h"

// 클라이언트로부터 데이터를 받아서 그대로 다시 클라이언트에게 보내는 함수
void echo (int connfd)
{
    // 1. 변수 선언
    size_t n;
    char buf[MAXLINE];
    rio_t rio;

    // 2. Rio 함수를 이용한 입력 초기화
    Rio_readinitb(&rio, connfd);
    
    // 3. 데이터 읽기 및 에코
    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0)
    {
        printf("바보server received %d bytes \n", (int)n);
        Rio_writen(connfd, buf, n);
    }
    
}

int main(int argc, char **argv)
{

    // 1. 변수 선언 and 초기화
    int listenfd, connfd; // 리스닝 소켓(서버가 클라이언트의 연결 요청 수신), 클라이언트 소켓의 파일 디스크립터
    socklen_t clientlen; // 클라이언트 주소 구조체의 크기를 저장하는 변수
    struct sockaddr_storage clientaddr; // 클라이언트의 주소 정보를 저장하는 범용 주소 구조체
    char client_hostname[MAXLINE], client_port[MAXLINE]; // 클라이언트의 호스트 이름 and 포트 번호를 문자열 형태로 저장

    // 2. 포트 번호 입력 확인
    if (argc != 2) 
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]); // 사용법 안내
        exit(0); 
    }

    // 3. 리스닝 소켓 열기
    listenfd = Open_listenfd(argv[1]); // 주어진 포트 번호를 리스닝 소켓을 열어서 파일 디스크립터 얻기

    // 4. 무한루프
    while (1)
    {
        // 클라이언트 주소 길이 초기화
        clientlen = sizeof(struct sockaddr_storage); 

        // 클라이언트의 연결 요청 수락, 새로운 연결 소켓 'connfd'를 생성'(클라이언트와의 통신에 사용)
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); 
        
        // 연결된 클라이언트의 주소 정보 해석, 호스트 이름 and 포인터 번호를 문자열로 가져옴.
        Getnameinfo((SA*) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0); 

        printf("Connected to (%s, %s) \n", client_hostname, client_port); // 연결된 클라이언트의 정보 출력
        
        echo(connfd); // 클라이언트로부터 데이터 입력 받고, 받은 후 데이터를 그대로 클라이언트에게 되돌려 보내기
        Close(connfd); // 클라이언트와 연결 종료 and 사용한 소켓 리소스 해제
    }

    exit(0);
    
}
