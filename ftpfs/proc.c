#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "ftpfs.h"

#define FTP_NAME_LEN      256
#define FTP_PORT_LEN      16
#define SPACES            "%*[ \t]"

#define FTP_OK            0
#define FTP_ERR           -1

/*
 * Get a line from stdin.
 */
static void get_line(char *line, size_t line_len)
{
  int c, i;

  for (i = 0;;) {
    /* get next character */
    c = fgetc(stdin);
    if (c == '\n')
      break;

    /* store character */
    if (c != '\r' && i < line_len - 1)
      line[i++] = c;
  }

  line[i] = 0;
}

/*
 * Ask user/password.
 */
struct ftp_param_t *ftp_ask_parameters()
{
  struct ftp_param_t *params;
  struct termios term;
  char *line = NULL;

  /* allocate parameters */
  params = (struct ftp_param_t *) malloc(sizeof(struct ftp_param_t));
  if (!params)
    return NULL;

  /* ask user */
  printf("User : ");
  get_line(params->user, FTPFS_NAME_LEN);

  /* get current tty */
  if (tcgetattr(fileno(stdin), &term))
    goto err;

  /* disable ECHO on tty */
  term.c_lflag &= ~ECHO;
  if (tcsetattr(fileno(stdin), TCSANOW, &term))
    goto err;

  /* ask password */
  printf("Password : ");
  get_line(params->passwd, FTPFS_NAME_LEN);

  /* enable ECHO on tty */
  term.c_lflag |= ECHO;
  if (tcsetattr(fileno(stdin), TCSANOW, &term))
    goto err;

  /* free line */
  free(line);

  return params;
err:
  if (line)
    free(line);
  free(params);
  return NULL;
}

/*
 * Build a FTP command.
 */
static void ftp_build_cmd(const char *cmd, const char *arg, char *buf, size_t len)
{
  if (arg)
    snprintf(buf, len, "%s %s\r\n", cmd, arg);
  else
    snprintf(buf, len, "%s\r\n", cmd);
}

/*
 * Get a line from FTP server.
 */
static int ftp_getline(int sockfd, char *buf, size_t len)
{
  int err, i;
  char c;

  if (!len)
    return FTP_OK;

  for (i = 0;;) {
    /* read next character */
    err = read(sockfd, &c, 1);
    if (err != 1)
      return FTP_ERR;

    /* end of line */
    if (c == '\n')
      break;

    /* store character */
    if (i < len)
      buf[i++] = c;
  }

  /* end buf with 0 */
  if (i < len)
    buf[i] = 0;
  else
    buf[i - 1] = 0;

  return FTP_OK;
}

/*
 * Get a reply from FTP server.
 */
static int ftp_getreply(int sockfd)
{
  char buf[BUFSIZ];
  int err, i;

  for (i = 0;; i++) {
    /* get next line */
    err = ftp_getline(sockfd, buf, BUFSIZ);
    if (err != FTP_OK)
      return err;

    /* print message */
    printf("FTP : %s\n", buf);

    /* break on FTP message */
    if (i == 0 && buf[3] != '-')
      break;
    if (i != 0 && isdigit(buf[0]) && isdigit(buf[1]) && isdigit(buf[2]) && buf[3] == ' ')
      break;
  }

  /* return FTP status code */
  return buf[0] - '0';
}

/*
 * Send a FTP command.
 */
static int ftp_cmd(int sockfd, const char *cmd, const char *arg)
{
  char buf[BUFSIZ];
  size_t len;

  /* build command */
  ftp_build_cmd(cmd, arg, buf, BUFSIZ);

  /* print message */
  printf("FTP : Send %s command\n", cmd);

  /* send message */
  len = strnlen(buf, BUFSIZ);
  if (write(sockfd, buf, len) != len) {
    fprintf(stderr, "FTP : can't send message\n");
    return FTP_ERR;
  }

  /* get reply */
  return ftp_getreply(sockfd);
}

/*
 * Open a data socket.
 */
static int ftp_opendatasock(int sockfd_ctrl, struct sockaddr *addr_ctrl)
{
  char name[FTP_NAME_LEN], port[FTP_PORT_LEN], buf[BUFSIZ];
  int sockfd, val = 1, err;
  struct sockaddr_in6 sa6;
  struct sockaddr_in sa4;
  struct sockaddr *addr;
  socklen_t len;

  /* create socket */
  sockfd = socket(addr_ctrl->sa_family, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("socket");
    return FTP_ERR;
  }

  /* set socket options */
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *) &val, sizeof(int)) < 0) {
    perror("setsockopt");
    close(sockfd);
    return FTP_ERR;
  }

  /* use dynamic port */
  switch (addr_ctrl->sa_family) {
    case AF_INET:
      memcpy(&sa4, addr_ctrl, sizeof(struct sockaddr_in));
      sa4.sin_port = 0;
      addr = (struct sockaddr *) &sa4;
      break;
    case AF_INET6:
      memcpy(&sa6, addr_ctrl, sizeof(struct sockaddr_in6));
      sa6.sin6_port = 0;
      addr = (struct sockaddr *) &sa6;
      break;
    default:
      close(sockfd);
      return FTP_ERR;
  }

  /* bind socket */
  len = sizeof(struct sockaddr);
  if (bind(sockfd, addr, len) < 0) {
    perror("bind");
    close(sockfd);
    return FTP_ERR;
  }

  /* get socket name */
  if (getsockname(sockfd, addr, &len) < 0) {
    perror("getsockname");
    close(sockfd);
    return FTP_ERR;
  }

  /* listen on socket */
  if (listen(sockfd, 1) < 0) {
    perror("listen");
    close(sockfd);
    return FTP_ERR;
  }

  /* prepare EPRT command = tells to server which port to use */
  getnameinfo(addr, sizeof(struct sockaddr), name, sizeof(name), port, sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV);
  switch (addr->sa_family) {
    case AF_INET:
      snprintf(buf, BUFSIZ, "|1|%s|%s|", name, port);
      break;
    case AF_INET6:
      snprintf(buf, BUFSIZ, "|2|%s|%s|", name, port);
      break;
    default:
      close(sockfd);
      return FTP_ERR;
  }

  /* send EPRT command */
  err = ftp_cmd(sockfd_ctrl, "EPRT", buf);
  if (err != 2) {
    close(sockfd);
    return FTP_ERR;
  }

  return sockfd;
}

/*
 * Write data to a FTP buffer.
 */
static int ftp_write_to_buf(const char *buf, int len, void *arg)
{
  /* get ftp buffer */
  struct ftp_buffer_t *ftp_buf = (struct ftp_buffer_t *) arg;

  /* grow buffer if needed */
  if (ftp_buf->len + len > ftp_buf->capacity) {
    ftp_buf->data = (char *) realloc(ftp_buf->data, ftp_buf->capacity + BUFSIZ);
    if (!ftp_buf->data)
      return FTP_ERR;

    ftp_buf->capacity += BUFSIZ;
  }

  /* copy to ftp buffer */
  memcpy(ftp_buf->data + ftp_buf->len, buf, len);
  ftp_buf->len += len;

  return FTP_OK;
}

/*
 * Write data to a file descriptor.
 */
static int ftp_write_to_fd(const char *buf, int len, void *arg)
{
  int fd = *((int *) arg), err;

  err = write(fd, buf, len);
  if (err != len)
    return FTP_ERR;

  return FTP_OK;
}

/*
 * Receive FTP data.
 */
static int ftp_receive_data(int sockfd_data, int (*ftp_write)(const char *, int, void *), void *arg)
{
  int sockfd, len, err;
  char buf[BUFSIZ];

  /* accept connection */
  sockfd = accept(sockfd_data, NULL, 0);
  if (sockfd < 0) {
    perror("accept");
    return FTP_ERR;
  }

  for (;;) {
    /* read next data */
    len = read(sockfd, buf, BUFSIZ);
    if (len < 0) {
      perror("read");
      close(sockfd);
      return FTP_ERR;
    }

    /* end of stream */
    if (len == 0)
      break;

    /* write data */
    err = ftp_write(buf, len, arg);
    if (err != FTP_OK)
      return err;
  }

  /* close socket */
  close(sockfd);

  return FTP_OK;
}

/*
 * Connect to a FTP server.
 */
int ftp_connect(const char *hostname, const char *user, const char *passwd, struct sockaddr *addr)
{
  char name[FTP_NAME_LEN], port[FTP_PORT_LEN];
  struct addrinfo *res, *nai;
  int err, sockfd;
  socklen_t len;

  /* get server informations */
  err = getaddrinfo(hostname, "ftp", NULL, &res);
  if (err)
    return FTP_ERR;

  /* try all names */
  for (nai = res; nai != NULL; nai = nai->ai_next) {
    /* create socket */
    sockfd = socket(nai->ai_family, nai->ai_socktype, nai->ai_protocol);
    if (sockfd < 0)
      continue;

    /* connect to server */
    if (connect(sockfd, nai->ai_addr, nai->ai_addrlen) < 0) {
      close(sockfd);
      continue;
    }

    /* get socket name */
    len = sizeof(struct sockaddr);
    if (getsockname(sockfd, addr, &len) < 0) {
      close(sockfd);
      continue;
    }

    /* print connection informations */
    getnameinfo(nai->ai_addr, nai->ai_addrlen, name, sizeof(name), port, sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV);
    freeaddrinfo(res);
    printf("FTP : connected to %s:%s\n", name, port);

    /* get FTP reply */
    err = ftp_getreply(sockfd);
    if (err != 2) {
      close(sockfd);
      return FTP_ERR;
    }

    /* send USER command */
    err = ftp_cmd(sockfd, "USER", user);
    if (err != 3) {
      close(sockfd);
      return FTP_ERR;
    }

    /* send PASS command */
    err = ftp_cmd(sockfd, "PASS", passwd);
    if (err != 2) {
      close(sockfd);
      return FTP_ERR;
    }

    /* set binary mode */
    err = ftp_cmd(sockfd, "TYPE", "I");
    if (err != 2) {
      close(sockfd);
      return FTP_ERR;
    }

    return sockfd;
  }

  freeaddrinfo(res);
  return FTP_ERR;
}

/*
 * Quit a FTP connection.
 */
int ftp_quit(int sockfd)
{
  int err;

  err = ftp_cmd(sockfd, "QUIT", NULL);
  if (err != 2)
    return FTP_ERR;

  return FTP_OK;
}

/*
 * List a FTP directory.
 */
int ftp_list(int sockfd, struct sockaddr *addr, const char *dir, struct ftp_buffer_t *buf)
{
  int sockfd_data, err;

  /* open a data socket */
  sockfd_data = ftp_opendatasock(sockfd, addr);
  if (sockfd_data < 0)
    return FTP_ERR;

  /* send list command */
  err = ftp_cmd(sockfd, "LIST", dir);
  if (err != 1) {
    close(sockfd_data);
    return FTP_ERR;
  }

  /* reset buffer length */
  buf->len = 0;

  /* receive data */
  err = ftp_receive_data(sockfd_data, ftp_write_to_buf, buf);
  if (err != FTP_OK) {
    close(sockfd_data);
    return err;
  }

  /* get reply */
  err = ftp_getreply(sockfd);
  if (err != 2) {
    close(sockfd_data);
    return FTP_ERR;
  }

  close(sockfd_data);
  return FTP_OK;
}

/*
 * Retrieve a file.
 */
int ftp_retrieve(int sockfd, struct sockaddr *addr, const char *pathname, int fd_out)
{
  int sockfd_data, err;

  /* open a data socket */
  sockfd_data = ftp_opendatasock(sockfd, addr);
  if (sockfd_data < 0)
    return FTP_ERR;

  /* send list command */
  err = ftp_cmd(sockfd, "RETR", pathname);
  if (err != 1) {
    close(sockfd_data);
    return FTP_ERR;
  }

  /* receive data */
  err = ftp_receive_data(sockfd_data, ftp_write_to_fd, &fd_out);
  if (err != FTP_OK) {
    close(sockfd_data);
    return err;
  }

  /* get reply */
  err = ftp_getreply(sockfd);
  if (err != 2) {
    close(sockfd_data);
    return FTP_ERR;
  }

  close(sockfd_data);
  return FTP_OK;
}

/*
 * Parse a FTP dir line.
 */
int ftp_parse_dir_line(const char *line, struct ftpfs_fattr_t *fattr)
{
  char mode[12], user[33], group[3], month[4], day[3], year[6], *link_marker;
  unsigned long nlinks = 1;
  unsigned long long size;
  int res, i;

  /* reset file attributes */
  memset(fattr->name, 0, FTPFS_NAME_LEN);
  memset(fattr->link, 0, FTPFS_NAME_LEN);
  memset(&fattr->statbuf, 0, sizeof(struct stat));

  /* parse line */
  res = sscanf(line,
               "%11s"
               "%lu"  SPACES
               "%32s" SPACES
               "%32s" SPACES
               "%llu" SPACES
               "%3s"  SPACES
               "%2s"  SPACES
               "%5s"  "%*c"
               "%1023c",
               mode, &nlinks, user, group, &size, month, day, year, fattr->name);

  /* not a directory entry */
  if (res < 9)
    return FTP_ERR;

  /* resolve link */
  link_marker = strstr(fattr->name, " -> ");
  if (link_marker) {
    strcpy(fattr->link, link_marker + 4);
    *link_marker = 0;
  }

  /* set statbuf */
  fattr->statbuf.st_nlink = nlinks;
  fattr->statbuf.st_size = size;

  /* parse mode */
  if (mode[0] == 'd')
    fattr->statbuf.st_mode |= S_IFDIR;
  else if (mode[0] == 'l')
    fattr->statbuf.st_mode |= S_IFLNK;
  else
    fattr->statbuf.st_mode |= S_IFREG;
  for (i = 1; i < 10; i++)
    if (mode[i] != '-')
      fattr->statbuf.st_mode |= 1 << (9 - i);

  return FTP_OK;
}

/*
 * Remove a file.
 */
int ftp_rm(int sockfd, const char *pathname)
{
  int err;

  err = ftp_cmd(sockfd, "DELE", pathname);
  if (err != 2)
    return FTP_ERR;

  return FTP_OK;
}

/*
 * Make a new directory.
 */
int ftp_mkdir(int sockfd, const char *pathname)
{
  int err;

  err = ftp_cmd(sockfd, "MKD", pathname);
  if (err != 2)
    return FTP_ERR;

  return FTP_OK;
}

/*
 * Remove a directory.
 */
int ftp_rmdir(int sockfd, const char *pathname)
{
  int err;

  err = ftp_cmd(sockfd, "RMD", pathname);
  if (err != 2)
    return FTP_ERR;

  return FTP_OK;
}
