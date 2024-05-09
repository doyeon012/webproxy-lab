/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char *method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

// 웹서버가 클라이언트로부터 받은 HTTP 요청을 해석하고,
// 그에 따라 적절한 콘텐츠(정적 or 동적)를 제공하는 역할 수행
void doit(int fd)
{

  // 1. 변수 초기화 및 설정
  int is_static; // 동적 or 정적 유무
  struct stat sbuf;
  
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  // 2. HTTP 요청 읽기
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);    // 요청라인을 읽고 분석

  printf("Request headers: \n");
  printf("%s", buf);

  // 3. 요청 메소드 검증
  sscanf(buf, "%s %s %s", method, uri, version);

  if (!(strcasecmp(method, "GET") == 0 || strcasecmp(method, "HEAD") == 0))        // POST만 받을꺼야 
  {
    clienterror(fd, method, "501", "Not impemented",
                "Tiny does not implement this method");
    return;
  }

  // 4. 요청 헤더 읽기
  read_requesthdrs(&rio);               // POST면 무시

  // 5. URI 파싱
  is_static = parse_uri(uri, filename, cgiargs);    // 정적 or 동적 인지
  
  // 6. 파일 상태 확인
  if (stat(filename, &sbuf) < 0)                    
  {
    clienterror(fd, filename, "404", "Not found",   
                "Tiny dees not implement this method");
    return;
  }


  if (is_static)      //Serve static content
  {
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))//보통 파일이 아니면서 GET 권한을 가지고 있지 않는
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }

    // 7 콘텐츠 제공
    // 7-1 정적 콘텐츠 제공
    serve_static(fd, filename, sbuf.st_size, method);     
  }
  else                // Serve dynamic content
  {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))//보통 파일이 아니면서 POST 권한을 가지고 있지 않는
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }

    // 7-2 동적 콘텐츠 제공
    serve_dynamic(fd, filename, cgiargs, method);       
  }

}

// 클라이언트의 HTTP 요청 처리 중 오류가 발생했을 때 사용되며, 적절한 HTTP 에러 메시지 생성 and 전송
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  // 1. HTTP 응답 본문 구성
  // 
  sprintf(body, "<html><title>Tiny Error</title>"); // body 버퍼에 HTML 내용 추가.
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n" , body); 
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause); 
  sprintf(body, "%s<hr><em>The tiny Web server</em>\r\n", body);

  // 2. HTTP 헤더 및 본문 전송
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf)); // 정보를 클라이언트에게 실제로 전송
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body)); 
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

// 클라이언트로부터 전송된 HTTP 요청 헤더를 읽어들이고 이를 로깅하며
// 무시 = 이 함수가 헤더 정보를 서버의 요청 처리 로직에 활용하지 않고 단지 읽기만 한다는 것
void read_requesthdrs(rio_t *rp)
{
  // 1. 변수 초기화
  char buf[MAXLINE];

  // 2. 첫 번째 헤더 라인 읽기
  Rio_readlineb(rp, buf, MAXLINE);

  // 3. 헤더 라인을 계속 읽고 출력
  while(strcmp(buf, "\r\n"))
  {
    rio_readlineb(rp, buf, MAXLINE); // 다음 헤더 라인을 읽고 'buf'에 저장
    printf("%s", buf);
  }

  return;
}

// 웹서버가 요청한 URL에 따라 적절한 콘텐츠 제공 방식을 선택할 수 있도록
// 정적 요청 or 동적 요청 구분 and 필요한 파일 경로나 or CGI 인자를 설정
int parse_uri(char *uri, char * filename, char *cgiargs){
  char *ptr;
  
  /* 과제 요건사항 : cgi-bin은 동적파일로 분류하자 */
  /* 만약 static content 요구라면, 1을 리턴한다. */

  // 1. URI 분석 및 분류
  if(!strstr(uri, "cgi-bin"))
  {

    // 2. 정적 콘텐츠(서버에 미리 저장되어 있는 파일 그대로 클라이언트에게 전달) 처리
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
        printf("%s\n",uri);
    if (uri[strlen(uri)-1] == '/') 
      strcat(filename, "home.html");
    return 1;
  }
else
  {
  
  // 3. 동적 콘텐츠(서버에 요청이 도착할 때마다 서버가 그 요청의 세부사항에 따라 콘텐츠를 생성 and 변경) 처리
  ptr = index(uri, '?'); // 이전(스크립트 파일 경로), 이후(스크립트에 전단될 인자)
  if (ptr){
    strcpy(cgiargs, ptr+1);
    *ptr = '\0';
  }
  else{
    strcpy(cgiargs, "");
  }
  strcpy(filename, ".");
  strcat(filename, uri);
  return 0;
  }
}

/*정적 컨텐츠를 클라이언트에게 제공*/
void serve_static(int fd, char *filename, int filesize, char *method)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  // 1. 파일 타입(MIME) 결정
  get_filetype(filename, filetype);         

  // 2. HTTP 응답 헤더 구성             
  sprintf(buf, "HTTP/1.0 200 OK\r\n"); // 성공적인 연결                      
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);

  // 3. HTTP 헤더 전송
  Rio_writen(fd, buf, strlen(buf));                          
  printf("Response headers: \n");
  printf("%s", buf);                                      

 
  // 4. 응답 본문 전송
  if (strcasecmp(method, "GET") == 0)
  {
    // 읽을 수 있는 파일로
    srcfd = Open(filename, O_RDONLY, 0);    

    // mmap = 파일 I/O 호출x, 메모리에 직접 데이터를 읽을 수 있어 입출력 성능이 크게up, 대용량. 시스템의 캐시 효과적 활용
    // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); //리눅스mmap함수를 요청한 파일을 가상메모리 영역으로 매핑
    // Close(srcfd);                      
    // Rio_writen(fd, srcp, filesize);         
    // Munmap(srcp, filesize);                 

    // malloc
    srcp = (char*)Malloc(filesize);
    Rio_readn(srcfd, srcp, filesize);
    Close(srcfd);
    
    Rio_writen(fd, srcp, filesize); //클라이언트에 전송
    free(srcp);
  }
}

// 파일의 확장자를 기반으로 해당 파일의 MIME타입 결정
void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))   // 'filename'에서 특정 확장자 검사
    strcpy(filetype, "text/html"); // 'filetype'에 MIME 타입 할당

  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");

  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");

  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpg");

  else if (strstr(filename, ".mp4"))
    strcpy(filetype, "video/mp4");

  else if (strstr(filename, ".mpg")) // MPG 비디오 파일3 처리
    strcpy(filetype, "video/mpeg");

  else
    strcpy(filetype, "text/plain"); // 일반 텍스트 파일
}

// CGI 프로그램을 실행하여 클라이언트로부터의 요청에 대응하여 동적으로 생성된 컨텐츠 제공
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method)
{
  char buf[MAXLINE], *emptylist[] = { NULL }; //포인터 배열

  // 1. HTTP 응답 준비
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  // 2. 자식 프로세스 생성
  if (Fork() == 0)                     
  {
    
    // 3. 환경 변수 설정
    setenv("QUERY_STRING", cgiargs, 1);   //QUERY_STRING 환경변수를 요청URI의 CGI 인자들로 초기화
    // setenv("REQUEST_METHOD", method, 1);
    
    // 4. 표준 출력 재지정(파일 기술자의 복사본 만들기) - 연결의 소켓으로 직접 보내기 위해
    Dup2(fd, STDOUT_FILENO);
    
    // 5. CGI 스크립트 실행 - 지정된 실행 파일을 메모리에 로드 and 실행.
    // 이전 프로그램의 모든 코드 and 데이터를 새 프로그램으로 대체
    Execve(filename, emptylist, environ); 
  }

  // 6. 자식 프로세스 종료 대기
  Wait(NULL);

}

/*
  fork()를 실행하면 부모프로세스와 자식 프로세스가 동시에 실행
  fork()의 반환값이 0 : 자식프로세스라면 if문을 수행
  fork()의 반환값이 0이 아니라면 : 내가 부모프로세스라면 if문을 수행하지 않고 Wait함수로 이동
  
  Wait() : 부모프로세스가 먼저 도달도 자식 프로세스가 종료될 때까지 기다리는 함수 
  if문 안에서 setnv 시스템콜을 수행해 "Query_String"의 값을 cgiargs로 바꿔준다.(우선순위 0순위)
  
  dup2() : CGI 프로세스 출력을 fd로 복사한다.
  dup2() : 실행 후 STDOUT_FILENO의 값은 fd이다. 
  dup2() : CGI 프로세스에서 표준 출력을 하면 바로 출력되지 않고 서버 연결 식별자를 거쳐 클라이언트 함수에 출력  
  
  execuv() : 파일이름이 첫번째 인자인 것과 같은 파일을 실행한다. 
*/

// 명령중 인자로 포트 번호를 입력 받아 해당 포트에서 클라이언트의 연결 요청을 수신 and 처리
int main(int argc, char **argv) 
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  // 1. 명령줄 인자 확인(2개 입력 받았는지)
  if (argc != 2) {           
    fprintf(stderr, "usage: %s <port>\n", argv[0]);     
    exit(1);
  }

  // 2. 소켓 열기 - 제공된 포트 번호에 대해 소켓을 열고, 이 소켓을 듣기 모드로 설정.
  // 듣기 소켓은(클라이언트로부터의 연결 요청을 수신할 준비가 된 상태)
  listenfd = Open_listenfd(argv[1]);      

  while (1) {
    clientlen = sizeof(clientaddr);

    // 3. 무한 루프에서 클라이언트 연결 수락
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); 

    // 4. 클라이언트 정보 획득
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);

    // 5. HTTP 요청 처리
    doit(connfd);  

    // 6. 연결 종료
    Close(connfd);  
  }
}
