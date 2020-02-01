#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // Usada para la comprobación del directorio pasado como parámetro
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <time.h>

#define VERSION 24
#define BUFSIZE 8096
#define ERROR 42
#define LOG 44

#define BAD_REQUEST 400
#define FORBIDDEN 403
#define NOT_FOUND 404
#define METHOD_NOT_ALLOWED 405
#define UNSUPPORTED_MEDIA_TYPE 415
#define HTTP_VERSION_NOT_SUPPORTED 505


char WORK_DIR[100] = {0}; //directorio de trabajo, se almacena el segundo parámetro

struct
{
      char *ext;
      char *filetype;
} extensions[] =
    {
        {"gif", "image/gif"},
        {"jpg", "image/jpg"},
        {"jpeg", "image/jpeg"},
        {"png", "image/png"},
        {"ico", "image/ico"},
        {"zip", "image/zip"},
        {"gz", "image/gz"},
        {"tar", "image/tar"},
        {"htm", "text/html"},
        {"html", "text/html"},
        {0, 0}};

void setDate(char *t);

void debug(int log_message_type, char *message, char *additional_info, int socket_fd)
{
      int fd;
      char logbuffer[BUFSIZE * 2];

      char response[BUFSIZE] = {0};
      char error_page[BUFSIZE] = {0};

      switch (log_message_type)
      {
      case ERROR:
            (void)sprintf(logbuffer, "ERROR: %s:%s Errno=%d exiting pid=%d", message, additional_info, errno, getpid());
            break;

      case BAD_REQUEST:
            // Enviar como respuesta 400 Bad Request
            strcpy(response, "HTTP/1.1 400 Bad Request\r\n");
            strcpy(error_page, "<html><head><title>ERROR</title></head><body bgcolor=\"lightgray\"></body><h1 align=\"center\"> 400 BAD REQUEST</h1><p align=\"center\">");
            strcat(error_page, additional_info);
            strcat(error_page, "</p></body></html>");
            (void)sprintf(logbuffer, "BAD_REQUEST: %s:%s", message, additional_info);
            break;

      case FORBIDDEN:
            // Enviar como respuesta 403 Forbidden
            strcpy(response, "HTTP/1.1 403 Forbidden\r\n");
            strcpy(error_page, "<html><head><title>ERROR</title></head><body bgcolor=\"lightgray\"></body><h1 align=\"center\"> 403 FORBIDDEN</h1><p align=\"center\">");
            strcat(error_page, additional_info);
            strcat(error_page, "</p></body></html>");
            (void)sprintf(logbuffer, "FORBIDDEN: %s:%s", message, additional_info);
            break;

      case NOT_FOUND:
            // Enviar como respuesta 404 Not Found
            strcpy(response, "HTTP/1.1 404 Not Found\r\n");
            strcpy(error_page, "<html><head><title>ERROR</title></head><body bgcolor=\"lightgray\"></body><h1 align=\"center\"> 404 NOT FOUND</h1><p align=\"center\">");
            strcat(error_page, additional_info);
            strcat(error_page, "</p></body></html>");
            (void)sprintf(logbuffer, "NOT FOUND: %s:%s", message, additional_info);
            break;

      case METHOD_NOT_ALLOWED:
            // Enviar como respuesta 405 Method Not Allowed
            strcpy(response, "HTTP/1.1 405 Method Not Allowed\r\n");
            strcpy(error_page, "<html><head><title>ERROR</title></head><body bgcolor=\"lightgray\"></body><h1 align=\"center\"> 405 METHOD NOT ALLOWED</h1><p align=\"center\">");
            strcat(error_page, additional_info);
            strcat(error_page, "</p></body></html>");
            (void)sprintf(logbuffer, "METHOD NOT ALLOWED: %s:%s", message, additional_info);
            break;

      case UNSUPPORTED_MEDIA_TYPE:
            // Enviar como respuesta 415 Unsupported Media Type
            strcpy(response, "HTTP/1.1 415 Unsupported Media Type\r\n");
            strcpy(error_page, "<html><head><title>ERROR</title></head><body bgcolor=\"lightgray\"></body><h1 align=\"center\"> 415 UNSUPPORTED MEDIA TYPE</h1><p align=\"center\">");
            strcat(error_page, additional_info);
            strcat(error_page, "</p></body></html>");
            (void)sprintf(logbuffer, " UNSUPPORTED MEDIA TYPE: %s:%s", message, additional_info);
            break;

      case HTTP_VERSION_NOT_SUPPORTED:
            // Enviar como respuesta 505 HTTP Version Not Supported
            strcpy(response, "HTTP/1.1 505 HTTP Version Not Supported\r\n");
            strcpy(error_page, "<html><head><title>ERROR</title></head><body bgcolor=\"lightgray\"></body><h1 align=\"center\"> 505 HTTP VERSION NOT SUPPORTED</h1><p align=\"center\">");
            strcat(error_page, additional_info);
            strcat(error_page, "</p></body></html>");
            (void)sprintf(logbuffer, "HTTP VERSION NOT SUPPORTED: %s:%s", message, additional_info);
            break;

      case LOG:
            (void)sprintf(logbuffer, " INFO: %s:%s:%d", message, additional_info, socket_fd);
            break;
      }

      if ((fd = open("webserver.log", O_CREAT | O_WRONLY | O_APPEND, 0644)) >= 0)
      {
            (void)write(fd, logbuffer, strlen(logbuffer));
            (void)write(fd, "\n", 1);
            (void)close(fd);
      }

      if (log_message_type != ERROR && log_message_type != LOG)  // en el resto de casos mandamos el error
      {
            // Mandamos la respuesta al cliente
            char datetime[80] = {0};
            setDate(datetime);
            strcat(response, datetime);
            strcat(response, "Server: SSTT_Apache\r\n");

            char content_length[80] = {0};
            sprintf(content_length, "Content-Length: %ld\r\n", strlen(error_page));
            strcat(response, content_length);

            strcat(response, "Content-Type: text/html ; charset=ISO-8859-1\r\n");
            strcat(response, "\r\n");

            strcat(response, error_page);
            (void)write(socket_fd, response, strlen(response));
      }
}

int isGetRequest(char *buffer, char *path_file, int socket_fd)
{

      /*
         Comprueba si el método de solicitud es correcto
       */

      char buffer_copy[BUFSIZE] = {0};
      strcpy(buffer_copy, buffer);

      char *method_line = strtok(buffer_copy, "@"); // cada línea fue separada por un "@", nos quedamos con la línea del get

      //
      // COMPROBACIÓN DEL TAMAÑO MÍNIMO
      //

      // comprobamos que tenga el tamaño mínimo permitido y descartamos mensajes del tipo "    GET / HTTP/1.1"
      if (strlen(method_line) < 14 || method_line[0] == ' ') //porque "GET / HTTP/1.1" == 14 chars  (mínimo que puede estar bien)
      {
            debug(BAD_REQUEST, "request", "La peticion no es correcta", socket_fd);
            return 0;
      }

      //
      // LECTURA DE CAMPOS
      //

      char *tok = strtok(method_line, " "); // lee el método de petición
      char method[20] = {0};                //guardamos el método para comprobar después si está bien, ya que primero comprobamos otros posibles errores
      strcpy(method, tok);

      tok = strtok(NULL, " ");          // lee el siguiente campo (la ruta)
      if (tok == NULL || tok[0] != '/') //debe comenzar por barra
      {
            debug(BAD_REQUEST, "request", "La peticion no es correcta", socket_fd);
            return 0;
      }
      else
      {
            strcpy(path_file, tok); // guardamos la ruta solicitada para usarla posteriormente
      }

      tok = strtok(NULL, " "); // lee la versión HTTP
      if (tok != NULL)         // se comprueba siempre que existan más campos
      {
            char version[20] = {0};
            strcpy(version, tok);

            if ((tok = strtok(NULL, " ")) != NULL) // el siguiente tok debe ser NULL, si no lo es es porque hay más de 3 campos, estaría mal
            {
                  debug(BAD_REQUEST, "request", "La peticion no es correcta", socket_fd);
                  return 0;
            }

            // si el formato es correcto, comprobamos primero el tipo de método y después si la versión se soporta
            if (strcmp(method, "GET") == 0)
            {
                  if (strcmp(version, "HTTP/1.1") == 0)
                  {
                        return 1; //CORRECTO
                  }
                  else
                  {
                        debug(HTTP_VERSION_NOT_SUPPORTED, "request", "La version HTTP no esta soportada", socket_fd);
                        return 0;
                  }
            }
            else
            {
                  debug(METHOD_NOT_ALLOWED, "request", "Metodo de solicitud no soportado", socket_fd); // en este caso, si ponemos "GOT / HTTP/1.1" lo rechazaría aquí
                  return 0;
            }
      }
      else
      {
            debug(BAD_REQUEST, "request", "La peticion no es correcta", socket_fd); // fallaría por tener solo 2 campos, por ejemplo "GET /"
            return 0;
      }
}

void setDate(char *t)
{
      time_t now = time(NULL);
      struct tm *date = localtime(&now);
      strftime(t, 80, "Date: %a, %d %b %Y %X GMT\r\n", date);
}

int isIllegalAccess(char *path_file)
{

      /*
         Para comprobar si accede a un directorio superior, cuando desciende sumamos 1 y cuando
         asciende restamos 1 de esta manera sabremos que si es <= -1  será un acceso ilegal.
       */

      char path_copy[100] = {0};
      int N = (int)strlen(path_file);

      for (int i = 1; i < N; i++)
            path_copy[i - 1] = path_file[i]; //de esta manera realizamos la copia para el strtok y quitamos el primer "/" que nos estorba

      char *tok = strtok(path_copy, "/");

      int level = 0; //cada directorio que descendemos sumará un nivel, y cada uno que sube ".." restará un nivel
      while (tok != NULL && level >= 0)
      {
            if (tok != NULL)
                  if (strcmp(tok, "..") != 0)
                        level++;
                  else
                        level--;
            tok = strtok(NULL, "/");
      }

      // solo si nos mantenemos en el mismo nivel de partida o en una subcarpeta, será válido
      if (level >= 0)
            return 1; // Es correcto

      return 0; //No es correcto
}

void setPathFile(char *path_file) // path_file contiene la ruta del fichero pedido en el HTTP GET request
{
      /*
         Para obtener la ruta real del fichero solicitado en el servidor, concatenaremos
         la ruta indicada en la cabecera GET, con la ruta del directorio de trabajo del servidor.
       */

      // obtiene la ruta absoluta en caso de ser relativa, como "." o "../", en caso de ser, por ejemplo, "/home/alumno/" la dejaría como "/home/alumno"
      char real_path[100];
      realpath(WORK_DIR, real_path);

      // "/" e "/index.html" son los casos directos donde se devuelve el index
      if (strcmp(path_file, "/") == 0)
            strcat(real_path, "/index.html");
      else
            strcat(real_path, path_file);

      strcpy(path_file, real_path);
}

int setContentType(char *content_type, char *path_file)
{
      /*
         Vamos a obtener la extensión del archivo solicitado y comprobar si se encuentra en el array de extensiones.
         Si existe coincidencia se forma la cabecera Content-Type y la variable "found" sera True (1) , en otro caso será False (0).
       */

      // obtenemos el nombre del fichero

      char path_copy[BUFSIZE] = {0};
      strcpy(path_copy, path_file);

      char *tok = strtok(path_copy, "/");
      char name_file[BUFSIZE] = {0};

      while (tok != NULL)
      {
            strcpy(name_file, tok);
            tok = strtok(NULL, "/");
      }

      char name_copy[BUFSIZE] = {0};
      strcpy(name_copy, name_file);

      //COMPROBAMOS SI TIENE EXTENSIÓN

      // Obtenemos la extensión del archivo solicitado
      char *extt = strtok(name_copy, ".");
      extt = strtok(NULL, "\0");

      if (extt == NULL)
            return 0; // si no contiene extensión no se soporta

      char ext[100] = {0};
      strcpy(ext, extt);

      //COMPROBAMOS SI LA EXTENSIÓN ES SOPORTADA POR EL SERVIDOR
      int i = 0;
      int found = 0; //False
      while ((extensions[i]).ext != 0)
      {
            if (strcmp(ext, (extensions[i]).ext) == 0)
            {
                  sprintf(content_type, "Content-Type: %s; charset=ISO-8859-1\r\n", (extensions[i]).filetype);
                  found = 1; // True
            }
            i++;
      }

      return found; // devolverá 0 si no se soporta el tipo de archivo, ya que no se encuentra extensión
}

int checkHeaders(char *buffer, int *cookie_counter)
{

      /*
         Comprueba si las cabeceras son correctas y por cada una es posible realizar una acción.
       */

      char buffer_copy[BUFSIZE] = {0}; // hacemos una copia para que no sea modificado por el strtok
      strcpy(buffer_copy, buffer);

      char *line = strtok(buffer_copy, "@");
      line = strtok(NULL, "@"); // saltamos la línea GET

      // guardamos todas las cabeceras en la estructura
      char headers[20][BUFSIZE] = {0};

      int i = 0;
      while (line != NULL)
      {
            strcpy(headers[i], line);
            line = strtok(NULL, "@");
            i++;
      }

      //
      //    TRATAMIENTO INDIVIDUAL PARA CADA CABECERA
      //    (no se realiza nada, pero se deja indicado con // TODO)
      //

      int N_headers_request = i - 1; // número de cabeceras de la petición

      for (int j = 0; j < N_headers_request; j++)
      {

            // Obtenemos el primer campo de la cabecera
            char header_copy[BUFSIZE] = {0};
            strcpy(header_copy, headers[j]);

            char *tok = strtok(header_copy, " ");

            char header[30] = {0}; // guardaremos aqui el nombre de la cabecera
            strcpy(header, tok);

            int n = strlen(tok);
            tok = strtok(header, ":");

            // Comprobamos que el formato de cabecera sea:   "NAME: "

            if (headers[j][n - 1] == ':' && headers[j][n] == ' ')
            {
                  if (strcmp(header, "Host") == 0)
                  {
                        // TODO
                  }

                  // resto de cabeceras

                  if (strcmp(header, "Cookie") == 0)
                  {
                        char header_copy2[BUFSIZE] = {0};
                        strcpy(header_copy2, headers[j]);

                        tok = strtok(header_copy2, "=");
                        char *value = strtok(NULL, "");
                        *cookie_counter = atoi(value);
                  }
            }
            else
            {
                  // Si entra aquí quiere decir que se ha producido un error y no sigue el formato "NAME: "
                  return 0;
            }
      }

      return 1; // si se procesan todas las cabeceras sin errores, termina correctamente
}

void process_web_request(int descriptorFichero)
{
      debug(LOG, "request", "Ha llegado una peticion", descriptorFichero);

      //
      // Definir buffer y variables necesarias para leer las peticiones
      //
      char buffer[BUFSIZE] = {0}; // almacenará la solicitud

      //
      // Leer la petición HTTP
      // Comprobación de errores de lectura
      //

      int bytes_read = 0;
      if ((bytes_read = read(descriptorFichero, buffer, BUFSIZE)) <= 0)
      {
            debug(BAD_REQUEST, "request", "Se ha producido un error de lectura", descriptorFichero);
            return;
      }
      //
      // Si la lectura tiene datos válidos terminar el buffer con un \0
      //
      buffer[bytes_read] = '\0';

      //
      // Traducimos los \r\n en un espacio y un asterisco, respectivamente
      //
      for (int i = 0; i < bytes_read; i++)
      {
            if (buffer[i] == '\r')
                  buffer[i] = ' ';
            else if (buffer[i] == '\n')
                  buffer[i] = '@';
      }

      //
      //	TRATAR LOS CASOS DE LOS DIFERENTES METODOS QUE SE USAN
      //	(Se soporta solo GET)
      //
      char path_file[BUFSIZE] = {0}; // guardaremos la ruta del fichero si lo encontramos
      if (isGetRequest(buffer, path_file, descriptorFichero) == 1)
      {
            //
            // COMPROBACIÓN DE LAS CABECERAS
            //
            int cookie_counter = 0; //para obtener el valor de la cookie si lo hubiese
            if (checkHeaders(buffer, &cookie_counter) == 0)
            {
                  debug(BAD_REQUEST, "request", "El formato de las cabeceras no son correctas", descriptorFichero);
                  return;
            }

            //
            //    ACCESOS ILEGALES
            //	Como se trata el caso de acceso ilegal a directorios superiores de la
            //	jerarquia de directorios del sistema
            //
            if (isIllegalAccess(path_file) != 1)
            {
                  debug(FORBIDDEN, "request", "Acceso ilegal a directorio superior", descriptorFichero);
                  return;
            }

            //
            //	Como se trata el caso excepcional de la URL que no apunta a ningún fichero html
            //
            //char name_file[BUFSIZE] = {0};
            setPathFile(path_file); // obtendremos el nombre del archivo para saber su extensión en el siguiente paso

            //
            //    EVALUACIÓN DEL RECURSO SOLICITADO
            //

            //	Evaluar el tipo de fichero que se está solicitando, y actuar en
            //	consecuencia devolviendolo si se soporta u devolviendo el error correspondiente en otro caso

            char content_type[BUFSIZE] = {0};
            if (setContentType(content_type, path_file) == 0)
            {
                  debug(UNSUPPORTED_MEDIA_TYPE, "request", "Extension no soportada por el servidor", descriptorFichero);
                  return;
            }

            // Se comprueba que el archivo exista
            if (access(path_file, F_OK) != 0)
            {
                  debug(NOT_FOUND, "request", "El fichero solicitado no existe", descriptorFichero);
                  return;
            }

            //
            //	En caso de que el fichero sea soportado, exista, etc. se envia el fichero con la cabecera
            //	correspondiente, y el envio del fichero se hace en bloques de un máximo de  8kB
            //

            //
            //GESTIÓN DE COOKIES: siempre que la petición GET sea correcta
            //
            char set_cookie[BUFSIZE] = {0};
            if (cookie_counter >= 10)
            {
                  debug(FORBIDDEN, "request", "Se ha alcanzado el limite de peticiones (cookie == 10)", descriptorFichero);
                  return;
            }
            else
            {
                  cookie_counter++;
                  sprintf(set_cookie, "Set-Cookie: sstt_cookie=%d; Max-Age=120\r\n", cookie_counter);
            }

            //
            //  CONSTRUCCIÓN Y ENVÍO DE LA RESPUESTA
            //

            // Leemos el contenido del fichero
            int file = open(path_file, O_RDONLY);

            // Establecemos la cabecera CONTENT-LENGTH
            char content_length[80] = {0};
            long cont_len = (long)lseek(file, (off_t)0, SEEK_END); // lee hasta el final del fichero
            lseek(file, (off_t)0, SEEK_SET);                       // devuelve el puntero al inicio
            sprintf(content_length, "Content-Length: %ld\r\n", cont_len);

            // Establecemos la cabecera DATE
            char datetime[80] = {0};
            setDate(datetime);

            // se forma la respuesta del servidor
            char response[BUFSIZE] = {0};
            strcat(response, "HTTP/1.1 200 OK\r\n");
            strcat(response, datetime);
            strcat(response, "Server: SSTT_Apache\r\n");
            strcat(response, set_cookie);
            strcat(response, content_length);
            strcat(response, content_type);
            strcat(response, "Keep-Alive: timeout=10, max=1000\r\n");
            strcat(response, "Connection: Keep-Alive\r\n");
            strcat(response, "\r\n");

            // mandamos la cabecera
            write(descriptorFichero, response, strlen(response));

            // mandamos los datos
            char data[BUFSIZE] = {0};
            long bytes_size;
            while ((bytes_size = read(file, data, BUFSIZE)) > 0)
                  write(descriptorFichero, data, bytes_size);

            /* Cierre de archivos */
            close(file);
      }

      // if (isPostRequest()) ...
      // if (isDeleteRequest()) ...
}

// por cada cliente hay un proceso hijo que gestiona la conexión
int main(int argc, char **argv)
{
      int i, port, pid, listenfd, socketfd;
      socklen_t length;
      static struct sockaddr_in cli_addr;  // static = Inicializado con ceros
      static struct sockaddr_in serv_addr; // static = Inicializado con ceros

      // Comprobamos que se hayan introducido 2 argumentos

      if (argc != 3)
      {
            printf("USO:   %s   <PUERTO>   <DIRECTORIO>  \n", argv[0]);
            exit(0);
      }

      //  Argumentos que se esperan:

      //	argv[1]
      //	En el primer argumento del programa se espera el puerto en el que el servidor escuchara

      if (atoi(argv[1]) == 0)
      {
            (void)printf("ERROR: \"%s\" no es un puerto\n", argv[1]);
            exit(0);
      }

      port = atoi(argv[1]);

      if (port < 1024 || port > 65536)
      {
            debug(ERROR, "Puerto invalido, prueba un puerto de 1024 a 65536", argv[1], 0);
            (void)printf("ERROR: \"%s\" no es un puerto válido\n", argv[1]);
            exit(0);
      }
      else
      {
            //  argv[2]
            //  En el segundo argumento del programa se espera el directorio en el que se encuentran los ficheros del servidor
            //  Verificar que el directorio escogido es apto. Que no es un directorio del sistema y que se tienen
            //  permisos para ser usado

            if (argv[2] != NULL)
            {
                  if (access(argv[2], F_OK) != 0)
                  {
                        debug(ERROR, "El directorio no existe", argv[2], 0);
                        (void)printf("ERROR: el directorio \"%s\" no existe  \n", argv[2]);
                        exit(0);
                  }

                  if (access(argv[2], W_OK) != 0)
                  {
                        debug(ERROR, "El directorio no tiene permisos de escritura", argv[2], 0);
                        (void)printf("ERROR: el directorio \"%s\" no tiene permisos de escritura\n", argv[2]);
                        exit(0);
                  }

                  if (access(argv[2], R_OK) != 0)
                  {
                        debug(ERROR, "El directorio no tiene permisos de lectura", argv[2], 0);
                        (void)printf("ERROR: el directorio \"%s\" no tiene permisos de lectura\n", argv[2]);
                        exit(0);
                  }

                  if (chdir(argv[2]) == -1)
                  {
                        debug(ERROR, "No se puede cambiar de directorio", argv[2], 0);
                        (void)printf("ERROR: No se puede cambiar de directorio \"%s\" \n", argv[2]);
                        exit(0);
                  }
            }
      }

      strcpy(WORK_DIR, argv[2]); // lo guardamos en una variable global

      // Hacemos que el proceso sea un demonio sin hijos zombies
      if (fork() != 0) // HACEMOS QUE PASE A SEGUNDO PLANO
            return 0;  // El proceso padre devuelve un OK al shell

      (void)signal(SIGCHLD, SIG_IGN); // Ignoramos a los hijos
      (void)signal(SIGHUP, SIG_IGN);  // Ignoramos cuelgues

      debug(LOG, "web server starting...", argv[1], getpid());

      /* setup the network socket */
      if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
            debug(ERROR, "system call", "socket", 0);

      /*Se crea una estructura para la información IP y puerto donde escucha el servidor*/
      serv_addr.sin_family = AF_INET;
      serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); /*Escucha en cualquier IP disponible*/ //transformar de formato host a formato red homogéneo ( los valores sobre la red se envían de una forma estandar)
      serv_addr.sin_port = htons(port);                                                     /*en el puerto port especificado como parámetro*/

      if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
            debug(ERROR, "system call", "bind", 0);

      if (listen(listenfd, 64) < 0) // socket que va a escuchar las conexiones tcp (lo pondremos en accept)
            debug(ERROR, "system call", "listen", 0);

      fd_set rfds; // conjunto de descriptores activos
      struct timeval tv;

      while (1)
      {
            length = sizeof(cli_addr);
            if ((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) < 0) // se quedará bloqueado aquí hasta que reciba una conexión tcp
                  debug(ERROR, "system call", "accept", 0);

            if ((pid = fork()) < 0)
                  debug(ERROR, "system call", "fork", 0);
            else
            {
                  if (pid == 0) // Proceso hijo
                  {
                        (void)close(listenfd); // para que no siga escuchando otras conexiones porque el fork hace copia del padre
                        while (1)
                        {
                              FD_ZERO(&rfds);          // inicializamos los descriptores
                              FD_SET(socketfd, &rfds); // lo añadimos al conjunto

                              tv.tv_sec = 10; // indicamos timeout de 10 segundos
                              tv.tv_usec = 0;

                              if (select(socketfd + 1, &rfds, NULL, NULL, &tv))
                                    process_web_request(socketfd); //se procesa la petición
                              else
                              {
                                    debug(LOG, "web server closed...", "timeout", getpid());
                                    FD_CLR(socketfd, &rfds);
                                    (void)close(socketfd);
                                    exit(0); // terminamos el proceso hijo
                              }
                        }
                  }
                  else                         // Proceso padre
                        (void)close(socketfd); // cierra el socket pues es el hijo quien se encarga de él, la conexión tcp seguirá activa en el hijo
            }
      }
}
