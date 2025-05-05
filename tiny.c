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
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv)     // C 프로그램의 시작점(엔트리 포인트)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1)                           // 무한 서버 루프를 실행
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,    // 반복적으로 연결 요청을 접수
                    &clientlen); // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);  // line:netp:tiny:doit       // 트랜잭션 수행
    Close(connfd); // line:netp:tiny:close      // 자신 쪽의 연결 끝 닫기
  }
}
// Tiny doit은 한 개의 HTTP 트랜잭션을 처리
void doit(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  Rio_readinitb(&rio, fd);    // 이 함수로 읽음
  Rio_readlineb(&rio, buf, MAXLINE);    //요청 라인 읽고 분석   
  printf("Request headers: \n");        //요청 라인 읽고 분석  
  printf("%s", buf);                    //요청 라인 읽고 분석  
  sscanf(buf, "%s %s %s", method, uri, version);    //요청 라인 읽고 분석  
  if (strcasecmp(method, "GET")){     // TINY는 GET 메소드만 지원 만약 다른 메소드 나오면 에러 메시지 보내고 main 함수로 되돌아 가서 연결을 닫고 연결 요청을 기다림
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio);

  is_static = parse_uri(uri, filename, cgiargs);    // 요청이 정적 또는 동적 컨텐츠를 위한 것인지 나타내는 플래그 설정
  if (stat(filename, &sbuf) < 0){
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  if (is_static){
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){     // 요청이 정적 컨텐츠 라면 이 파일이 보통 파일 이라는 것 + 읽기 권한을 가지고 있는지 검증
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename,sbuf.st_size);    // 만일 그렇다면 정적 컨텐츠를 클라이언트에게 제공
  }
  else {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)){     // 요청이 동적 컨텐츠라면 이 파일이 실행 가능한지 검증
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs);   // 만일 그렇다면 동적 컨텐츠를 클라이언트에게 제공
  }
}

/* clienterror 함수는 HTTP 응답을 응답 라인에 적절한 상태 코드와 상태 메시지와 함께 클라이언트에 보냄
브라우저 사용자에게 에러를 설명하는 응답 본체에 HTML 파일도 함께 보냄*/
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) 
{
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body*/
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  /* Print the HTTP response*/
  sprintf(buf, "HTTP/1.0 %s %s \r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

// read_requesthdrs 함수가 요청 헤더를 읽고 무시하는 동작을 수행
/* 클라이언트가 보낸 요청을 한 줄씩 읽기
읽은 줄이 \r\n이면 (= 빈 줄이면), 요청 헤더가 끝났다는 뜻
그 전까지는 계속 헤더를 읽고 출력만 함 (printf("%s", buf);)*/
/* HTTP 파싱할 때, 요청 헤더와 본문을 나누는 기준이 되기 때문에 중요 */
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while(strcmp(buf, "\r\n")){         // '\r\n'은 HTTP에서 줄의 끝을 의미하며, 빈 줄(\r\n)은 요청 헤더의 끝을 뜻함
    Rio_readlineb(rp, buf, MAXLINE);  // \r은 carriage return (줄 맨 앞으로), \n은 line feed (다음 줄로), 둘을 합친 \r\n은 Windows 스타일 줄바꿈이며, HTTP에서 표준 줄 구분자
    printf("%s", buf);
  }
  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  // URI에 "cgi-bin"이 없으면 정적 컨텐츠로 간주 (예: HTML, 이미지 파일 등)
  if (!strstr(uri, "cgi-bin")){      // cgi-bin은 웹 서버에서 CGI (Common Gateway Interface) 프로그램이 저장되는 디렉토리
    strcpy(cgiargs, "");             // CGI 인자는 필요 없으므로 비움
    strcpy(filename, ".");           // 현재 디렉토리에서 시작
    strcat(filename, uri);           // URI를 이어 붙여 파일 경로 생성
    if (uri[strlen(uri)-1] == '/')   // URI가 '/'로 끝나면 디폴트 파일로 home.html을 붙임
      strcat(filename, "home.html");
    return 1;  // 정적 컨텐츠라는 의미
  }
  else {  // "cgi-bin"이 포함되면 동적 컨텐츠로 간주 (예: 실행 가능한 CGI 프로그램)
    ptr = index(uri, '?');       // '?' 문자가 있으면 CGI 인자 시작점
    if (ptr){
      strcpy(cgiargs, ptr+1);    // '?' 다음부터 끝까지를 CGI 인자로 복사
      *ptr = '\0';               // '?' 위치를 널 문자로 바꿔 uri 문자열을 자름
    }
    else
      strcpy(cgiargs, "");       // '?'가 없으면 CGI 인자 없음
    strcpy(filename, ".");       // 현재 디렉토리에서 시작
    strcat(filename, uri);       // 남은 URI로 파일 경로 생성 (예: ./cgi-bin/adder)
    return 0;  // 동적 컨텐츠라는 의미
  }
}

/* Tiny 웹서버의 serve_static() 함수에서 정적 파일을 클라이언트에게 보내는 기존 방식은 
mmap()을 사용하여 파일을 메모리에 매핑하고 그 내용을 Rio_writen()으로 보내는 구조*/
/*void serve_static(int fd, char *filename, int filesize)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  // 응답 헤더 만들기
  get_filetype(filename, filetype);               // MIME 타입 결정 (예: text/html)
  sprintf(buf, "HTTP/1.0 200 OK\r\n");            // 상태 라인
  sprintf(buf, "%sServer : Tiny Web Server\r\n", buf);      // 서버 정보
  sprintf(buf, "%sConnection: close\r\n", buf);             // 연결 종료 알림
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);  // 컨텐츠 길이
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype); // MIME 타입
  Rio_writen(fd, buf, strlen(buf));               // 헤더 전송
  printf("Response headers:\n");
  printf("%s", buf);                              // 서버 로그로 헤더 출력

  // 응답 바디 보내기
  srcfd = Open(filename, O_RDONLY, 0);            // 파일 열기 (읽기 전용)
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);  // 파일 메모리 매핑
  Close(srcfd);                                   // 파일 디스크립터 닫기
  Rio_writen(fd, srcp, filesize);                 // 클라이언트에 파일 내용 전송
  Munmap(srcp, filesize);                         // 매핑 해제
}*/

void serve_static(int fd, char *filename, int filesize)
{
  int srcfd;
  char *srcbuf, filetype[MAXLINE], buf[MAXBUF];

  // 응답 헤더 생성
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf + strlen(buf), "Server: Tiny Web Server\r\n");
  sprintf(buf + strlen(buf), "Connection: close\r\n");
  sprintf(buf + strlen(buf), "Content-length: %d\r\n", filesize);
  sprintf(buf + strlen(buf), "Content-type: %s\r\n\r\n", filetype);
  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers:\n");
  printf("%s", buf);

  // 파일 내용 읽어서 전송 (mmap 없이)
  srcfd = Open(filename, O_RDONLY, 0);
  srcbuf = (char *)Malloc(filesize);               // 동적 메모리 할당
  Rio_readn(srcfd, srcbuf, filesize);              // 파일에서 버퍼로 읽기
  Close(srcfd);
  Rio_writen(fd, srcbuf, filesize);                // 클라이언트에게 전송
  free(srcbuf);                                    // 버퍼 메모리 해제
}
/* get_filetype = Derive file type from filename*/
void get_filetype(char *filename, char * filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");      // HTML 파일이면 text/html
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");      // GIF 이미지
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");      // PNG 이미지
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");     // JPG 이미지
  else if (strstr(filename, ".mpg")){   // 최신 브라우저는 mpg 파일을 지원하지 않아서 mp4로 대체
      strcpy(filetype, "video/mpeg");
  }
  else
    strcpy(filetype, "text/plain");     // 그 외는 일반 텍스트
}

void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = {NULL};  // CGI 인자로 전달할 리스트 (빈 리스트)

  // 응답 헤더 전송 (본문은 CGI 프로그램이 직접 출력함)
  sprintf(buf, "HTTP/1.0 200 OK\r\n");      // 상태 라인
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");  // 서버 정보
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0){  // 자식 프로세스
    setenv("QUERY_STRING", cgiargs, 1);     // CGI 인자를 환경 변수로 설정
    Dup2(fd, STDOUT_FILENO);                // stdout을 클라이언트로 리디렉션
    Execve(filename, emptylist, environ);   // CGI 프로그램 실행
  }
  Wait(NULL);  // 부모는 자식이 끝날 때까지 기다림
}