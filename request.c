#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <openssl/ssl.h>


struct retVal {
    int n;
    int code;
};

int socket_connect(char *host, in_port_t port){
	struct hostent *hp;
	struct sockaddr_in addr;
	int on = 1, sock;     

	if ((hp = gethostbyname(host)) == NULL){
		herror("gethostbyname");
		return -1;
	}

	bcopy(hp->h_addr, &addr.sin_addr, hp->h_length);
	addr.sin_port = htons(port);
	addr.sin_family = AF_INET;
	sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&on, sizeof(int));

	if (sock == -1){
		perror("setsockopt");
		return -1;
	}


	if (connect(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1){
		perror("connect");
		return -1;

	}

	return sock;
}

int comp(const void * elem1, const void * elem2) 
{
    int f = *((int*)elem1);
    int s = *((int*)elem2);
    if (f > s) return  1;
    if (f < s) return -1;
    return 0;
}

struct retVal request(char const *argv[]){
    
    char sendline[4096 + 1], recvline[4096 + 1];
    char* ptr;

    char OutResponse[4096 + 1];
    int MAXRESPONSE = 4096;
    int MAXLINE = 10;

    // default return value unless bytes read is set
    size_t n = -1;

    // seperate the url and pathname
    char *url = strtok((char *) argv[1], "/");
    char *path = NULL;
    int i = 0;
    while( url != NULL ) {
        if (i == 1){
            path = strtok(NULL, " ");
            break;
        }
        i++;
    }

    int sockfd = socket_connect((char *) argv[1], atoi(argv[2])); 

    if (sockfd == -1){
        struct retVal r = { -1, -1 };
		return r;
    }

    // connect without ssl
    if (strcmp(argv[2], "80") == 0){

        // set the path name
        if (path == NULL){
            path = "/";
        }
        else{
            char *t = "/";
            size_t len = strlen(t);
            memmove(path + len, path, strlen(path) + 1);
            memcpy(path, t, len);
        }

        // // Form request
        snprintf(sendline, 4096, 
            "GET %s HTTP/1.1\r\n"  // POST or GET, both tested and works. Both HTTP 1.0 HTTP 1.1 works, but sometimes 
            "Host: %s\r\n"     // but sometimes HTTP 1.0 works better in localhost type
            "\r\n", path, argv[1]);


        /// Write the request
        if (write(sockfd, sendline, strlen(sendline))>= 0) 
        {                
            // / Read the response
            n = read(sockfd, recvline, 4096);
            recvline[n] = '\0';

            if(fputs(recvline, stdout) == EOF)
            {
                printf("fputs() error\n");
            }


            /// Remove the trailing chars
            ptr = strstr(recvline, "\r\n\r\n");
            
            // check len for OutResponse here ?
            snprintf(OutResponse, 4096,"%s", ptr);

        }   
    }
    // connect securely using ssl
    else{

        SSL_load_error_strings ();
        SSL_library_init ();
        SSL_CTX *ssl_ctx = SSL_CTX_new (SSLv23_client_method ());

        SSL *conn = SSL_new(ssl_ctx);
        SSL_set_fd(conn, sockfd);

        int err = SSL_connect(conn);
        if (err != 1){
            abort();
        }


        // set the path name
        if (path == NULL){
            path = "/";
        }
        else{
            char *t = "/";
            size_t len = strlen(t);
            memmove(path + len, path, strlen(path) + 1);
            memcpy(path, t, len);
        }

        // // Form request
        snprintf(sendline, 4096, 
            "GET %s HTTP/1.1\r\n"  // POST or GET, both tested and works. Both HTTP 1.0 HTTP 1.1 works, but sometimes 
            "Host: %s\r\n"     // but sometimes HTTP 1.0 works better in localhost type
            "\r\n", path, argv[1]);


        /// Write the request
        if (SSL_write(conn, sendline, strlen(sendline)) >= 0) 
        {               
            
            /// Read the response
            n = SSL_read(conn, recvline, 4096);
            recvline[n] = '\0';

            if(fputs(recvline, stdout) == EOF)
            {
                printf("fputs() error\n");
            }


            /// Remove the trailing chars
            ptr = strstr(recvline, "\r\n\r\n");
            
            // check len for OutResponse here ?
            snprintf(OutResponse, 4096,"%s", ptr);

        }   
    }

    char* s;
    char httpCode[3 * __SIZEOF_WCHAR_T__];
    for (i = 9; i < 12; i++ )
    {
        s = &recvline[i];
        httpCode[i - 9] = *s;
    }
    
    // puts(httpCode);

    // int returnVals[2 * __SIZEOF_INT__];
    // returnVals[0] = n;
    // returnVals[1] = atoi(httpCode);
    struct retVal r = { n, atoi(httpCode) };

    return r;
}

int main(int argc, char const *argv[])
{   

    // provide the user with instructions on how to use the command line tool
    if (strcmp(argv[1], "help") == 0) {
        puts("This program is a Command Line tool to perform HTTP requests similar to CURL.\n");
        puts("Usage: ./request <url> <portNumber> <profile> <numRequests>\n");
        puts("--url, specify the url of the website you would like to request from\n");
        puts("--portNumber, specify the portNumber, 80 for HTTP, 443 for HTTPS\n");
        puts("--profile, if profile is an argument, the requests will be performed multiple times and a data report will be provided at the end\n");
        puts("--numRequests, specify the number of times you would like the request to be performed\n");
        puts("example: ./request www.google.com 443 profile 10");

        return 0;
    }

    // profile option is on, profile the url
    if (argc >=4 && strcmp(argv[3], "profile") == 0){
        puts("profiling the selected url");
        puts("\n");
        int iterations = atoi(argv[4]);

        clock_t times[iterations];
        int bytesReadOrFailure[iterations];
        int httpCodes[iterations];

        for (int i = 0; i < iterations; i++){
            clock_t start = clock(), diff;

            // bytesReadOrFailure[i] = request(argv);
            struct retVal response = request(argv);
            bytesReadOrFailure[i] = response.n;
            httpCodes[i] = response.code;

            diff = clock() - start;

            times[i] = diff;
        }
        
        int minTime = INT_MAX;
        int maxTime = 0;
        int totalTime = 0;

        int minBytes = INT_MAX;
        int maxBytes = 0;
        int failures = 0;

        for (int i = 0; i < iterations; i++){
            if (times[i] < minTime){
                minTime = times[i];
            }
            if (times[i] > maxTime){
                maxTime = times[i];
            }
            totalTime += times[i];

            if (bytesReadOrFailure[i] < minBytes){
                minBytes = bytesReadOrFailure[i];
            }

            if (bytesReadOrFailure[i] > maxBytes){
                maxBytes = bytesReadOrFailure[i];
            }

            if (bytesReadOrFailure[i] == -1){
                failures += 1;
            }
        }

        puts("\n");
        printf("PROFILE OF %s\n", argv[1]);
        puts("\n");
        printf("NUMBER OF REQUESTS: %d\n", iterations);
        printf("SLOWEST TIME: %d\n", maxTime);
        printf("FASTEST TIME: %d\n", minTime);
        printf("AVERAGE TIME: %d\n", totalTime / iterations);

        int median;
        qsort(times, iterations, sizeof(clock_t), comp);
        if (iterations % 2 == 0){
            median = times[iterations/2] + times[iterations/2 + 1];
        }
        else{
            median = times[iterations/2 + 1];
        }

        printf("MEDIAN TIME: %d\n", median);     

        printf("%% OF REQUESTS WHICH SUCCEEDED: %d\n", (iterations - failures)/iterations * 100);
        printf("ERROR CODES:\n");
        for (int i =0; i < iterations; i++){
            if (httpCodes[i] != 200){
                printf("%d\n", httpCodes[i]);
            }
        }
        printf("SIZE OF SMALLEST REQUEST: %d\n", minBytes);
        printf("SIZE OF LARGEST REQUEST: %d\n", maxBytes);

    }
    // profile option is not on, simply request from the url one time
    else{
        request(argv);
    }
    
    
    return 0;
}
