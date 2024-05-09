#include <stdio.h>
#include <signal.h>

#include "csapp.h"
#include "cache.h"

void *thread(void *vargp);
void doit(int clientfd);
void read_requesthdrs(rio_t *rp, void *buf, int serverfd, char *hostname, char *port);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void parse_uri(char *uri, char *hostname, char *port, char *path);


static const int is_local_test = 1; // 테스트 환경에 따른 도메인&포트 지정을 위한 상수 (0 할당 시 도메인&포트가 고정되어 외부에서 접속 가능)
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";


// 네트워크 소켓을 설정 and 클라이언트로부터의 연결을 계속해서 수락하여 각 연결을 별도의 스레드에서 처리
int main(int argc, char **argv)
    {
      // 1. 변수 선언 or 초기화
      int listenfd, *clientfd;
      char client_hostname[MAXLINE], client_port[MAXLINE];
      socklen_t clientlen;
      struct sockaddr_storage clientaddr;
      pthread_t tid;

      // 2. 시그널 핸들링
      signal(SIGPIPE, SIG_IGN); // SIGPIPE 예외처리

      // 3. 캐시 초기화
      rootp = (web_object_t *)calloc(1, sizeof(web_object_t)); // 캐시 연결 리스트의 루트
      lastp = (web_object_t *)calloc(1, sizeof(web_object_t)); // 마지막 노드를 위한 메모리 할당
    
      if (argc != 2)
      {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
      }

      // 4. 포트 및 리스닝 소켓 설정
      listenfd = Open_listenfd(argv[1]); // 전달받은 포트 번호를 사용해 수신 소켓 생성
      
      // 5. 클라이언트 연결 수락 루프
      while (1)
      {
        clientlen = sizeof(clientaddr);
        clientfd = Malloc(sizeof(int));

        // 들어오는 클라이언트 연결 수락
        *clientfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); 
        
        Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", client_hostname, client_port);
        
        // 별도의 스레드에서 처리됨. 동시성을 제공하여 여러 클라이언트를 동시에 처리 가능
        Pthread_create(&tid, NULL, thread, clientfd); 
      }
    }


// 각 클라이언트의 연결을 처리하는 별도의 스레드에서 실행
void *thread(void *vargp)
{
  // 1. 소켓 파일 디스크립터 추출
  int clientfd = *((int *)vargp);

  // 2. 스레드 분리
  Pthread_detach(pthread_self());

  // 3. 소켓 파일 디스크립터 포인터 메모리 해제
  Free(vargp);

  // 4. 클라이언트 요청 처리
  doit(clientfd);
  Close(clientfd);
  return NULL;
}


// 클라이언트의 요청을 받아들이고, 적절한 웹 서버로 전달하여 그 응답을 다시 클라이언트에게 전송
// 또한, 캐싱된 내용이 있을 경우 서버에 요청을 보내지 않고 캐시된 데이터를 사용하여 효율성을 높인다.
void doit(int clientfd)
{
  int serverfd, content_length;
  char request_buf[MAXLINE], response_buf[MAXLINE];
  char method[MAXLINE], uri[MAXLINE], path[MAXLINE], hostname[MAXLINE], port[MAXLINE];
  char *response_ptr, filename[MAXLINE], cgiargs[MAXLINE];
  rio_t request_rio, response_rio;


  // 1. 클라이언트 요청 읽기
  // 1️ - 1. 리퀘스트 라인(읽기) [Client -> Proxy] 
  Rio_readinitb(&request_rio, clientfd);
  Rio_readlineb(&request_rio, request_buf, MAXLINE);
  printf("Request headers:\n %s\n", request_buf);

  // 요청 파싱
  sscanf(request_buf, "%s %s", method, uri); // 메서드 and url 추출
  parse_uri(uri, hostname, port, path); // 분리

  // 요청 재구성 - HTTP/1.0 버전만 처리한다고 가정
  sprintf(request_buf, "%s %s %s\r\n", method, path, "HTTP/1.0");


  // 메서드 검사
  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD"))
  {
    clienterror(clientfd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }

  // 캐시 검사
  web_object_t *cached_object = find_cache(path);
  if (cached_object) // 캐싱o
  {
    send_cache(cached_object, clientfd); // 캐싱된 객체를 Client에 전송
    read_cache(cached_object);           // 사용한 웹 객체의 순서(맨 앞으로) 갱신
    return;                              
  }



  // 서버 요청 전송
  // 1️ - 2. 리퀘스트(전송) [Proxy -> Server] 
  // Server 소켓 생성
  serverfd = is_local_test ? Open_clientfd(hostname, port) : Open_clientfd("52.79.234.188", port);
  if (serverfd < 0)
  {
    clienterror(serverfd, method, "502", "Bad Gateway", "📍 Failed to establish connection with the end server");
    return;
  }
  Rio_writen(serverfd, request_buf, strlen(request_buf));

  // 2️. 리퀘스트 헤더(읽기 & 전송) [Client -> Proxy -> Server] 
  read_requesthdrs(&request_rio, request_buf, serverfd, hostname, port);

  // 서버 응답 처리
  // 3️. 리스펀스(헤더) (읽기 & 전송) [Server -> Proxy -> Client]
  Rio_readinitb(&response_rio, serverfd);
  while (strcmp(response_buf, "\r\n"))
  {
    Rio_readlineb(&response_rio, response_buf, MAXLINE);
    if (strstr(response_buf, "Content-length")) // Response Body 수신에 사용하기 위해 Content-length 저장
      content_length = atoi(strchr(response_buf, ':') + 1);
    Rio_writen(clientfd, response_buf, strlen(response_buf));
  }

  // 4️. 리스펀스(바디) (읽기 & 전송) [Server -> Proxy -> Client] 
  response_ptr = malloc(content_length);
  Rio_readnb(&response_rio, response_ptr, content_length);
  Rio_writen(clientfd, response_ptr, content_length); // Client에 Response Body 전송

  if (content_length <= MAX_OBJECT_SIZE) // 캐싱 가능한 크기인 경우
  {
    // `web_object` 구조체 생성
    web_object_t *web_object = (web_object_t *)calloc(1, sizeof(web_object_t));
    web_object->response_ptr = response_ptr;
    web_object->content_length = content_length;
    strcpy(web_object->path, path);
    write_cache(web_object); // 캐시 연결 리스트에 추가
  }
  else
    free(response_ptr); // 캐싱하지 않은 경우만 메모리 반환

  Close(serverfd);
}

// 클라이언트에 에러 전송
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  // 1. HTML 오류 페이지 생성
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor="
                "ffffff"
                ">\r\n",
          body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  // 2. HTTP 오류 헤더 구성
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));

  // 3. 오류 헤더 및 본문 전송
  Rio_writen(fd, body, strlen(body));
}

// uri를 `hostname`, `port`, `path`로 파싱하는 함수
// uri 형태: `http://hostname:port/path` 혹은 `http://hostname/path` (port는 optional)
void parse_uri(char *uri, char *hostname, char *port, char *path)
{
  // host_name의 시작 위치 포인터: '//'가 있으면 //뒤(ptr+2)부터, 없으면 uri 처음부터
  // 1. URL 분석 시작 위치 찾기
  char *hostname_ptr = strstr(uri, "//") ? strstr(uri, "//") + 2 : uri;
  
  // 2. 포트 번호 위치 찾기
  char *port_ptr = strchr(hostname_ptr, ':'); // port 시작 위치 (없으면 NULL)
  
  /// 3. 경로 위치 찾기
  char *path_ptr = strchr(hostname_ptr, '/'); // path 시작 위치 (없으면 NULL)
  
  // 4. 경로 복사
  strcpy(path, path_ptr);


  // 5. 포트 번호 및 호스트 이름 추출
  if (port_ptr) // port o
  {
    strncpy(port, port_ptr + 1, path_ptr - port_ptr - 1); 
    strncpy(hostname, hostname_ptr, port_ptr - hostname_ptr);
  }
  else // port x
  {
    if (is_local_test)
      strcpy(port, "80"); // port의 기본 값인 80으로 설정
    else
      strcpy(port, "8000");
    strncpy(hostname, hostname_ptr, path_ptr - hostname_ptr);
  }
}

// 프록시 서버가 클라이언트로부터 받은 요청을 원서버로 안정적으로 전달하고, 요청 및 응답 사이의 호환성을 보장
// 클라이언트의 요청을 적절히 조정하고, 필요한 정보를 보완하여 원 서버와의 효과적인 통신을 지원
void read_requesthdrs(rio_t *request_rio, void *request_buf, int serverfd, char *hostname, char *port)
{
  int is_host_exist;
  int is_connection_exist;
  int is_proxy_connection_exist;
  int is_user_agent_exist;


  // 1. 헤더 읽기
  Rio_readlineb(request_rio, request_buf, MAXLINE); 

  // 2. 헤더 검사 및 수정
  while (strcmp(request_buf, "\r\n"))
  {
    if (strstr(request_buf, "Proxy-Connection") != NULL)
    {
      sprintf(request_buf, "Proxy-Connection: close\r\n");
      is_proxy_connection_exist = 1;
    }
    else if (strstr(request_buf, "Connection") != NULL)
    {
      sprintf(request_buf, "Connection: close\r\n");
      is_connection_exist = 1;
    }
    else if (strstr(request_buf, "User-Agent") != NULL)
    {
      sprintf(request_buf, user_agent_hdr);
      is_user_agent_exist = 1;
    }
    else if (strstr(request_buf, "Host") != NULL)
    {
      is_host_exist = 1;
    }

    Rio_writen(serverfd, request_buf, strlen(request_buf)); // Server에 전송
    Rio_readlineb(request_rio, request_buf, MAXLINE);       // 다음 줄 읽기
  }

  // 3. 필수 헤더 추가
  if (!is_proxy_connection_exist)
  {
    sprintf(request_buf, "Proxy-Connection: close\r\n");
    Rio_writen(serverfd, request_buf, strlen(request_buf));
  }
  if (!is_connection_exist)
  {
    sprintf(request_buf, "Connection: close\r\n");
    Rio_writen(serverfd, request_buf, strlen(request_buf));
  }
  if (!is_host_exist)
  {
    if (!is_local_test)
      hostname = "52.79.234.188";
    sprintf(request_buf, "Host: %s:%s\r\n", hostname, port);
    Rio_writen(serverfd, request_buf, strlen(request_buf));
  }
  if (!is_user_agent_exist)
  {
    sprintf(request_buf, user_agent_hdr);
    Rio_writen(serverfd, request_buf, strlen(request_buf));
  }


  // 4. 헤더 전송 종료
  sprintf(request_buf, "\r\n"); // 종료문
  Rio_writen(serverfd, request_buf, strlen(request_buf));
  return;
}