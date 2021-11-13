/*
 * matelib.c
 *
 *  Created on: 5 oct. 2021
 *      Author: utnso
 */


#include "matelib.h"

//int main(void) {
//	 mate_instance* lib = malloc(sizeof(mate_instance));
//	 int status = mate_init(lib, CONFIG_PATH);
//
//	 if(status != 0) {
//		 printf("Algo salio mal");
//	     exit(-1);
//	 }
//
//	 printf("rey");
//	 enviar_mensaje("hola como estas", CONEXION);
//
//	return EXIT_SUCCESS;
//}

void pedir_permiso_para_continuar(int conexion) {

	t_paquete* paquete = malloc(sizeof(t_paquete));

	int* permiso = malloc(sizeof(int));

	(*permiso) = 1;

	paquete->codigo_operacion = PERMISO_CONTINUACION;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = sizeof(int);
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, &permiso, paquete->buffer->size);

	int bytes;

	void* a_enviar = serializar_paquete(paquete, &bytes);

	send(conexion, a_enviar, bytes, 0);

	free(a_enviar);
	free(permiso);
	eliminar_paquete(paquete);

}

void recibir_permiso_para_continuar(int conexion) {

	recibir_operacion(conexion);
	//char* mensaje = recibir_mensaje(conexion);
	//printf("Permiso para continuar: %s\n", mensaje);

	void* buffer;
	int size;

	recv(conexion, &size, sizeof(int), MSG_WAITALL);
	buffer = malloc(size);
	recv(conexion, buffer, size, MSG_WAITALL);

	int* mensaje = malloc(size);

	printf("Llegoooooo\n");

	memcpy(mensaje, buffer, size);


	printf("Permiso para continuar: %d\n", (*mensaje));

	free(mensaje);

}

int recibir_operacion(int socket_cliente) {
   peticion_carpincho cod_op;
   if (recv(socket_cliente, &cod_op, sizeof(peticion_carpincho), MSG_WAITALL) != 0) {
      return cod_op;
   }
   else
   {
      close(socket_cliente);
      return -1;
   }
}

void enviar_mensaje(char* mensaje, int socket_cliente)
{
	t_paquete* paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = MENSAJE;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = strlen(mensaje) + 1;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, mensaje, paquete->buffer->size);

	int bytes;

	void* a_enviar = serializar_paquete(paquete, &bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	eliminar_paquete(paquete);
}



void* serializar_paquete(t_paquete* paquete, int* bytes)
{

   int size_serializado = sizeof(peticion_carpincho) + sizeof(int) + paquete->buffer->size;
   void *buffer = malloc(size_serializado);

   int offset = 0;

   memcpy(buffer + offset, &paquete->codigo_operacion, sizeof(int));
   offset+= sizeof(int);
   memcpy(buffer + offset, &paquete->buffer->size, sizeof(int));
   offset+= sizeof(int);
   memcpy(buffer + offset, paquete->buffer->stream, paquete->buffer->size);
   offset+= paquete->buffer->size;

   (*bytes) = size_serializado;
   return buffer;
}

void eliminar_paquete(t_paquete* paquete)
{
   free(paquete->buffer->stream);
   free(paquete->buffer);
   free(paquete);
}


char* recibir_mensaje(int socket_cliente) {
   int size;
   char* buffer = recibir_buffer(&size, socket_cliente);
   return buffer;
}

void* recibir_buffer(int* size, int socket_cliente) {
   void* buffer;

   recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
   buffer = malloc(*size);
   recv(socket_cliente, buffer, *size, MSG_WAITALL);

   return buffer;
}



//------------------General Functions---------------------/

int mate_init(mate_instance *lib_ref, char *config)
{
	t_lib_config lib_config = crear_archivo_config_lib(config);

	int conexion;

    //Conexiones
	conexion = crear_conexion_kernel(lib_config.ip_kernel, lib_config.puerto_kernel);

	if(conexion == (-1)) {

		        printf("No se pudo conectar con el Kernel, inicio de conexion con Memoria");

		        conexion = crear_conexion_kernel(lib_config.ip_memoria, lib_config.puerto_memoria);

		        if(conexion == (-1)) {

		        		        printf("Error al conectar con Memoria");
		        		        return 1;
		    }
	}

  //Reserva memoria del tamanio de estructura administrativa
  lib_ref->group_info = malloc(sizeof(mate_inner_structure));

  if(lib_ref->group_info == NULL){
	  return 1;
  }

  ((mate_inner_structure *)lib_ref->group_info)->socket_conexion = conexion;

  printf("conexion: %d\n", conexion);

  pedir_permiso_para_continuar(((mate_inner_structure *)lib_ref->group_info)->socket_conexion);

  printf("Ya pedi permiso\n");

  recibir_permiso_para_continuar(((mate_inner_structure *)lib_ref->group_info)->socket_conexion);

  printf("Chau me re bloquee \n");

  return 0;
}

int mate_close(mate_instance *lib_ref)
{
	close(((mate_inner_structure *)lib_ref->group_info)->socket_conexion);
	free(lib_ref->group_info);
	return 0;
}

t_lib_config crear_archivo_config_lib(char* ruta) {
    t_config* lib_config;
    lib_config = config_create(ruta);
    t_lib_config config;

    if (lib_config == NULL) {
        printf("No se pudo leer el archivo de configuracion de Lib\n");
        exit(-1);
    }

    config.ip_kernel = config_get_string_value(lib_config, "IP_KERNEL");
    config.puerto_kernel = config_get_string_value(lib_config, "PUERTO_KERNEL");
    config.ip_memoria = config_get_string_value(lib_config, "IP_MEMORIA");
    config.puerto_memoria = config_get_string_value(lib_config, "PUERTO_MEMORIA");

    return config;
}

int crear_conexion(char *ip, char* puerto)
{
   struct addrinfo hints;
   struct addrinfo *server_info;

   memset(&hints, 0, sizeof(hints));
   hints.ai_family = AF_UNSPEC;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_flags = AI_PASSIVE;

   getaddrinfo(ip, puerto, &hints, &server_info);

   int socket_cliente = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);

   if(connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen) == -1)
      printf("No se pudo conectar\n");

   freeaddrinfo(server_info);

   return socket_cliente;
}

int crear_conexion_kernel(char *ip, char* puerto)
{
	struct addrinfo hints;
	struct addrinfo *server_info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	getaddrinfo(ip, puerto, &hints, &server_info);

	int socket_cliente = socket(server_info->ai_family,
	                    server_info->ai_socktype,
	                    server_info->ai_protocol);

	connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen);

	freeaddrinfo(server_info);

	return socket_cliente;
}


void mate_printf() {
	printf("Aca esta la matelib imprimiendo algo\n");
}

//-----------------Semaphore Functions---------------------/

int mate_sem_init(mate_instance *lib_ref, mate_sem_name sem, unsigned int value) {
	t_paquete* paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = INICIALIZAR_SEM;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = strlen(sem) + 1 + sizeof(unsigned int);
	paquete->buffer->stream = malloc(paquete->buffer->size);
	int offset = 0;
	memcpy(paquete->buffer->stream + offset, sem, strlen(sem) + 1);
	offset += strlen(sem) + 1;
	memcpy(paquete->buffer->stream + offset, &value, sizeof(unsigned int));
	offset += sizeof(unsigned int);

	int bytes;

	void* a_enviar = serializar_paquete(paquete, &bytes);

	send(((mate_inner_structure *)lib_ref->group_info)->socket_conexion, a_enviar, bytes, 0);

	printf("yo tambien llegue hasta aqui");

	recibir_permiso_para_continuar(((mate_inner_structure *)lib_ref->group_info)->socket_conexion);

	//fflush(stdout);
	free(a_enviar);
	eliminar_paquete(paquete);
	return 0;
}

int mate_sem_wait(mate_instance *lib_ref, mate_sem_name sem) {
	t_paquete* paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = ESPERAR_SEM;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = strlen(sem) + 1;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	int offset = 0;
	memcpy(paquete->buffer->stream + offset, sem, strlen(sem) + 1);
	offset += strlen(sem) + 1;

	int bytes;

	void* a_enviar = serializar_paquete(paquete, &bytes);

	send(((mate_inner_structure *)lib_ref->group_info)->socket_conexion, a_enviar, bytes, 0);

	recibir_permiso_para_continuar(((mate_inner_structure *)lib_ref->group_info)->socket_conexion);

	free(a_enviar);
	eliminar_paquete(paquete);
	return 0;

}

int mate_sem_post(mate_instance *lib_ref, mate_sem_name sem) {
  if (strncmp(sem, "SEM1", 4))
  {
    return -1;
  }
  return sem_post(((mate_inner_structure *)lib_ref->group_info)->sem_instance);
}

int mate_sem_destroy(mate_instance *lib_ref, mate_sem_name sem) {
  if (strncmp(sem, "SEM1", 4))
  {
    return -1;
  }
  int res = sem_destroy(((mate_inner_structure *)lib_ref->group_info)->sem_instance);
  free(((mate_inner_structure *)lib_ref->group_info)->sem_instance);
  return res;
}

//--------------------IO Functions------------------------/

int mate_call_io(mate_instance *lib_ref, mate_io_resource io, void *msg)
{
  printf("Doing IO %s...\n", io);
  usleep(10 * 1000);
  if (!strncmp(io, "PRINTER", 7))
  {
    printf("Printing content: %s\n", (char *)msg);
  }
  printf("Done with IO %s\n", io);
  return 0;
}

//--------------Memory Module Functions-------------------/

mate_pointer mate_memalloc(mate_instance *lib_ref, int size)
{
  ((mate_inner_structure *)lib_ref->group_info)->memory = malloc(size);
  return 0;
}

int mate_memfree(mate_instance *lib_ref, mate_pointer addr)
{
  if (addr != 0)
  {
    return -1;
  }
  free(((mate_inner_structure *)lib_ref->group_info)->memory);
  return 0;
}

int mate_memread(mate_instance *lib_ref, mate_pointer origin, void *dest, int size)
{
  if (origin != 0)
  {
    return -1;
  }
  memcpy(dest, ((mate_inner_structure *)lib_ref->group_info)->memory, size);
  return 0;
}

int mate_memwrite(mate_instance *lib_ref, void *origin, mate_pointer dest, int size)
{
  if (dest != 0)
  {
    return -1;
  }
  memcpy(((mate_inner_structure *)lib_ref->group_info)->memory, origin, size);
  return 0;
}
