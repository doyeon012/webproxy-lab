#include "csapp.h"

int main(int argc, char **argv)
{
    int clientfd;
    char *host, *port, buf[MAXLINE];
    rio_t rio ;

    if (argc != 3) // 3개의 인자를 받지 못했을 경우
    {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]); // 올바른 사용법 안내 메시지(출력)
        exit(0); 
    }

    host = argv[1]; // 호스트명
    port = argv[2]; // 포트 번호

    // 2. 서버 연결 설정
    clientfd = Open_clientfd(host, port); // 함수 호출하여 서버 연결. 내부적 소켓 생성 and 서버의 주소 정보 확인 > 
                                          // connect() 시스템 콜 사용 서버와의 연결 설정. 성공시 사용할 소켓 파일 디스크립터 반환
    // 3. 입출력 초기화
    Rio_readinitb(&rio, clientfd); // 구조체 초기화 > 버퍼링된 입출력 작업 준비

    // 4. 메인 루프(사용자 입력 and 서버 응답 처리)
    while (Fgets(buf, MAXLINE, stdin) != NULL) //
    {
       Rio_writen(clientfd, buf, strlen(buf)); // 버퍼링된 출력 함수로, 내부 버퍼에서 파일디스크립터로 데이터를 즉시 전송
       
       Rio_readlineb(&rio, buf, MAXLINE); // 버퍼링된 입력 함ㅈ수로, 내부 버퍼에서 한 줄을 읽어들인다.
                                          // 내부적으로 'Rio_read()'를 호출하여 필요시 버퍼를 채운다.
    
       Fputs(buf, stdout); 
    }

    // 5. 리소스 정리
    Close(clientfd); // 함수를 호출. 소켓 연결 종료 and 관련 리소스 해제
    exit(0);
}