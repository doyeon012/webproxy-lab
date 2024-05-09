
#include "../csapp.h"

int main(void)
{
  
  // 1. 변수 선언
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 0;

  // 2. 쿼리 스트링 파싱
  if ((buf = getenv("QUERY_STRING")) != NULL)
  {
    p = strchr(buf, '&');
    *p = '\0';

    strcpy(arg1, buf); // 하나의 문자열을 다른 문자열로 복사
    strcpy(arg2, p + 1);

    n1 = atoi(strchr(arg1, '=') + 1); // 문자열 > 정수로 변환
    n2 = atoi(strchr(arg2, '=') + 1);

    sscanf(buf, "first=%d", &n1);
    sscanf(p+1, "second=%d", &n2);
  }

  // 3. HTTP 응답 본문 생성
  sprintf(content, "QUERY_STRING=%s", buf); // 정수 and 실수 'buffer'에 저장, 화면에 출력
  sprintf(content, "Welcome to add.com: ");
  sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content);
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>",
          content, n1, n2, n1 + n2);
  sprintf(content, "%sThanks for visiting!\r\n", content);

  // 4. HTTP 헤더 출력
  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  
  // 5. HTTP 응답 본문 출력
  printf("%s", content);
  fflush(stdout);

  exit(0);
}
/* $end adder */