/*
 ============================================================================
 Name        : memoria.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include "memoria.h"

int main(void) {

	init_memoria();

	printf("%i" , sizeof(heap_metadata));
	//enviar_mensaje("hola como estas", SERVIDOR_MEMORIA);
	//coordinador_multihilo();

	//memalloc(10,0);

	return EXIT_SUCCESS;
}


//--------------------------------------------------------------------------------------------

void init_memoria(){

	//Inicializamos logger
	LOGGER = log_create("memoria.log", "MEMORIA", 0, LOG_LEVEL_INFO);

	//Levantamos archivo de configuracion
	CONFIG = crear_archivo_config_memoria("/home/utnso/workspace/tp-2021-2c-DesacatadOS/memoria/src/memoria.config");

	//Iniciamos servidor
	SERVIDOR_MEMORIA = iniciar_servidor(CONFIG.ip_memoria, CONFIG.puerto_memoria);

	//Instanciamos memoria principal
	MEMORIA_PRINCIPAL = malloc(CONFIG.tamanio_memoria);

	if (MEMORIA_PRINCIPAL == NULL) {
	   	perror("MALLOC FAIL!\n");
	   	log_info(LOGGER,"No se pudo alocar correctamente la memoria principal.");
        return;
	}

	//Senales
	signal(SIGINT,  &signal_metricas);
	signal(SIGUSR1, &signal_dump);
	signal(SIGUSR2, &signal_clean_tlb);

	//Iniciamos paginacion
	iniciar_paginacion();
}


void coordinador_multihilo(){

	while(1) {

		int socket = esperar_cliente(SERVIDOR_MEMORIA);

		log_info(LOGGER, "Se conecto un cliente %i\n", socket);

		pthread_t* hilo_atender_carpincho = malloc(sizeof(pthread_t));
		pthread_create(hilo_atender_carpincho, NULL, (void*)atender_carpinchos, (void*)socket);
		pthread_detach(*hilo_atender_carpincho);
	}
}

void atender_carpinchos(int cliente) {

	peticion_carpincho operacion = recibir_operacion(cliente);

	switch (operacion) {

	case MEMALLOC:;
		/*int* pid  = malloc(4);
		pid = recibir_entero();
		size = recibir_entero();
		*/

		int size = 0;
		int pid = 0;

		memalloc(size , pid);
	break;

	case MEMREAD:;
	break;

	case MEMFREE:;
	break;

	case MEMWRITE:;
	break;

	case MENSAJE:;
		char* mensaje = recibir_mensaje(cliente);
		printf("%s",mensaje);
		fflush(stdout);
	break;

	//Faltan algunos cases;
	default:; break;

	}
}

void iniciar_paginacion() {

	int cant_frames_ppal = CONFIG.tamanio_memoria / CONFIG.tamanio_pagina;

	log_info(LOGGER, "Tengo %d marcos de %d bytes en memoria principal", cant_frames_ppal, CONFIG.tamanio_pagina);

	//Creamos lista de tablas de paginas
	TABLAS_DE_PAGINAS = list_create();

	//Creamos lista de frames de memoria ppal.
	MARCOS_MEMORIA = list_create();

	for (int i = 0 ; i < cant_frames_ppal ; i++) {

		t_frame* frame = malloc(sizeof(t_frame));
		frame->ocupado = 0;
		frame->id = i;

		list_add(MARCOS_MEMORIA, frame);
	}
}

int memalloc(int pid , int size){
	int dir_logica = -1;

	//[CASO A]: Llega un proceso nuevo
	if(list_get(TABLAS_DE_PAGINAS , pid) == NULL){

	if(alocar_en_swap(pid , size) == -1 ){
		printf("No se puede asginar %i bytes cantidad de memoria al proceso %i (cant maxima) ", size, pid);
		log_info(LOGGER, "No se puede asginar %i bytes cantidad de memoria al proceso %i (cant maxima) ", size, pid);

		return -1;
	}

	int marcos_necesarios = ceil(size / CONFIG.tamanio_pagina);

	//Crea tabla de paginas para el proceso
	t_tabla_pagina* nueva_tabla = malloc(sizeof(t_tabla_pagina));
	nueva_tabla->paginas = list_create();
	nueva_tabla->PID = pid;

	pthread_mutex_lock(&mutexTablas);
	list_add(TABLAS_DE_PAGINAS, nueva_tabla);
	pthread_mutex_unlock(&mutexTablas);

	//inicializo header inicial
	heap_metadata* header = malloc(sizeof(heap_metadata));
	header->is_free = false;
	header->next_alloc = sizeof(heap_metadata) + size;
	header->prev_alloc = NULL;

	//armo el alloc siguiente
	heap_metadata* header_siguiente = malloc(sizeof(heap_metadata));
	header->is_free = true;
	header->next_alloc = NULL;
	header->prev_alloc = 0;

	//Bloque en donde se meteran los headers
	void* marquinhos = malloc(marcos_necesarios * CONFIG.tamanio_pagina);

	memcpy(marquinhos, header, sizeof(heap_metadata));
	memcpy(marquinhos + sizeof(heap_metadata) + size, header_siguiente, sizeof(heap_metadata));

	//Copia del bloque 'marquinhos' a memoria;
	serializar_paginas_en_memoria(nueva_tabla->paginas, marquinhos);

	dir_logica = header_siguiente->prev_alloc;

	free(header);
	free(header_siguiente);

	//crea las paginas y las guarda segun asignacion y algoritmos
	guardar_paginas_en_memoria(pid , marcos_necesarios , nueva_tabla->paginas , marquinhos);

	}

	else
	//[CASO B]: El proceso existe en memoria
	{
		int alloc = obtener_alloc_disponible(pid, size,0);

		if (alloc != -1) {
			guardar_alloc_en_memoria(pid, size, alloc);

			return alloc;
		}

		// hay que agregar paginas
		// verificar si el proceso puede tener mas dependiendo el algoritmo de reemplazo


	}

	return dir_logica;
}
//TODO: Ver si estamos devolviendo la DL o DF
//TODO: Poner lock a la pagina para que no me la saque otro proceso mientras la uso
//TODO: Ver los memcpy xq podrian estar en 2 paginas distintas :/
int obtener_alloc_disponible(int pid, int size, uint32_t posicion_heap_actual) {

	t_list* paginas_proceso = ((t_tabla_pagina*)list_get(TABLAS_DE_PAGINAS, pid))->paginas;
	int nro_pagina = 0, offset = 0;

	nro_pagina = ceil(posicion_heap_actual / CONFIG.tamanio_pagina);
	offset = posicion_heap_actual - CONFIG.tamanio_pagina * nro_pagina;

	t_pagina* pag = malloc(sizeof(t_pagina));
	pag = list_get(paginas_proceso, nro_pagina);

	if (!pag->presencia) {
		//Si esta en principal == 1
		//TODO: lo traigo :: traer_pagina_a_memoria();
	}

	heap_metadata* header = desserializar_header(pid, nro_pagina, offset);

	if (header->is_free) {

		//----------------------------------//
		//  [Caso A]: El next alloc es NULL
		//----------------------------------//

		if(header->next_alloc == NULL){

			int tamanio_total = list_size(paginas_proceso) * CONFIG.tamanio_pagina;

			if (tamanio_total - posicion_heap_actual - sizeof(heap_metadata) * 2 > size) {

				//Creo header nuevo
				heap_metadata* header_nuevo =   malloc(sizeof(heap_metadata));
				header_nuevo->is_free = true;
				header_nuevo->next_alloc = NULL;
				header_nuevo->prev_alloc = posicion_heap_actual;

				//Actualizo header viejo
				header->is_free = false;
				header->next_alloc = posicion_heap_actual + sizeof(heap_metadata) + size;

				//Copio 1ero el header viejo actualizado y despues el header nuevo
				guardar_header(pid, nro_pagina, offset, header);
				guardar_header(pid, nro_pagina, offset + size, header_nuevo);
				return header->next_alloc;
			}

			return -1;

		//-------------------------------------//
		//  [Caso B]: El next alloc NO es NULL
		//-------------------------------------//

		} else {

			// B.A. Entra justo, reutilizo el alloc libre
			if (header->next_alloc - posicion_heap_actual - sizeof(heap_metadata) == size) {
				header->is_free = false;
				guardar_header(pid, nro_pagina, offset, header);
				return posicion_heap_actual;

			// B.B. Sobra espacio, hay que meter un header nuevo
			}  else if(header->next_alloc - posicion_heap_actual - sizeof(heap_metadata)*2 > size) {

				//Pag. en donde vamos a insertar el nuevo header
				int nro_pagina_nueva = 0, offset_pagina_nueva = 0;
				nro_pagina_nueva = ceil((posicion_heap_actual + size + 9) / CONFIG.tamanio_pagina);
				offset_pagina_nueva = posicion_heap_actual + size + 9 - CONFIG.tamanio_pagina * nro_pagina_nueva;

				t_pagina* pag_nueva = malloc(sizeof(t_pagina));
				pag_nueva = (t_pagina*)list_get(paginas_proceso, nro_pagina_nueva);

				if (!pag_nueva->presencia) { //Si esta en principal == 1
					//TODO: lo traigo
				}

				//Pag. en donde esta el header siguiente al header final (puede ser la misma pag. que la anterior)
				int nro_pagina_final = 0, offset_pagina_final = 0;
				nro_pagina_final = ceil((header->next_alloc) / CONFIG.tamanio_pagina);

				offset_pagina_final = header->next_alloc - CONFIG.tamanio_pagina * nro_pagina_nueva;

				t_pagina* pag_final = malloc(sizeof(t_pagina));
				pag_final = (t_pagina*)list_get(paginas_proceso, nro_pagina_final);

				if (!pag_nueva->presencia) { //Si esta en principal == 1
					//TODO: lo traigo
				}

				heap_metadata* header_final = desserializar_header(pid, nro_pagina_final, offset_pagina_final);

				//Creo header nuevo y lo guardo
				heap_metadata* header_nuevo = malloc(sizeof(heap_metadata));
				header_nuevo->is_free = true;
				header_nuevo->prev_alloc = posicion_heap_actual;
				header_nuevo->next_alloc = header->next_alloc;
				guardar_header(pid, nro_pagina_nueva, offset_pagina_nueva, header_nuevo);

				//Actualizo y guardo los headers que ya estaban.
				header->next_alloc = posicion_heap_actual + sizeof(heap_metadata) + size;
				header_final->prev_alloc = posicion_heap_actual + sizeof(heap_metadata) + size;
				guardar_header(pid, nro_pagina, offset, header);
				guardar_header(pid, nro_pagina_final, offset_pagina_final, header_final);

			}

			obtener_alloc_disponible(pid,size,header->next_alloc);
		}
	}

	 // No esta free
	if (header->next_alloc != NULL){
		obtener_alloc_disponible(pid,size,header->next_alloc);
	}

	 return -1;
}

void guardar_header(int pid, int nro_pagina, int offset, heap_metadata* header){

	t_list* paginas_proceso = ((t_tabla_pagina*)list_get(TABLAS_DE_PAGINAS, pid))->paginas;
	t_pagina* pagina = (t_pagina*)list_get(paginas_proceso, nro_pagina);

	int diferencia = CONFIG.tamanio_pagina - offset - sizeof(heap_metadata);

	//El header entra completo en la pagina
	if(diferencia >= 0){
		memcpy(MEMORIA_PRINCIPAL + CONFIG.tamanio_pagina * pagina->frame_ppal + offset, header, sizeof(heap_metadata));

	} else {
		diferencia = abs(diferencia);
		t_pagina* pag_sig = (t_pagina*)list_get(paginas_proceso, nro_pagina+1);
		memcpy(MEMORIA_PRINCIPAL + CONFIG.tamanio_pagina * pagina->frame_ppal + offset, header, sizeof(heap_metadata) - diferencia);
		memcpy(MEMORIA_PRINCIPAL + CONFIG.tamanio_pagina * pag_sig->frame_ppal, header + sizeof(heap_metadata) - diferencia, diferencia);
	}
}


int guardar_paginas_en_memoria( int pid , int marcos_necesarios , t_list* paginas , void* contenido ){

	//chequear esquema de asignacion
	//chequear espacio -> swapear otras paginas en ese caso
	//guardar en paginas libres de a una pagina (hacer un for para guardar y swapear de a una)
	//serializar contenido en la memoria posta
	//Copia del bloque 'marquinhos' a memoria; ->	serializar_paginas_en_memoria(nueva_tabla->paginas, marquinhos);
	return 0;
}

int guardar_alloc_en_memoria(pid, size, alloc){

	return 0;
}

int alocar_en_swap(pid , size){
	//abrir conexion
	//enviar datos
	//chequear respuesta ( 1 o -1)

	return 0;
}

int obtener_ultimo_header(int pid) {
	void* marquinhos_proceso = ((t_tabla_pagina*)list_get(TABLAS_DE_PAGINAS, pid))->paginas;

/*
	for (int i = 0 ; i < list_size(paginas_proceso) ; i++) {
		int frame_aux = ((t_pagina*)list_get(paginas_proceso, i))->frame_ppal;
		void* header_buffer = malloc(sizeof(heap_metadata));
		memcpy(header_buffer, MEMORIA_PRINCIPAL + frame_aux * CONFIG.tamanio_pagina, CONFIG.tamanio_pagina);
	}
*/
}

void* traer_marquinhos_del_proceso(int pid) {

	t_list* paginas_proceso = ((t_tabla_pagina*)list_get(TABLAS_DE_PAGINAS, pid))->paginas;

	void* buffer = malloc(sizeof(CONFIG.tamanio_pagina) * list_size(paginas_proceso));

	for (int i = 0 ; i < list_size(paginas_proceso) ; i++) {
		int frame_aux = ((t_pagina*)list_get(paginas_proceso, i))->frame_ppal;
		memcpy(buffer + i * CONFIG.tamanio_pagina, MEMORIA_PRINCIPAL + frame_aux * CONFIG.tamanio_pagina, CONFIG.tamanio_pagina);
	}

	return buffer;
}

heap_metadata* desserializar_header(int pid, int nro_pag, int offset_header) {
	int offset = 0;
	heap_metadata* header = malloc(sizeof(heap_metadata));
	void* buffer = malloc(sizeof(heap_metadata));

	t_list* pags_proceso = ((t_tabla_pagina*)list_get(TABLAS_DE_PAGINAS, pid))->paginas;
	t_pagina* pagina = (t_pagina*) list_get(pags_proceso, nro_pag);

	int diferencia = CONFIG.tamanio_pagina - offset_header - sizeof(heap_metadata);
	if (diferencia >= 0) {
		memcpy(buffer, MEMORIA_PRINCIPAL + pagina->frame_ppal * CONFIG.tamanio_pagina + offset_header, sizeof(heap_metadata));
	} else {
		t_pagina* pagina_sig = (t_pagina*) list_get(pags_proceso, nro_pag+1);
		diferencia = abs(diferencia);
		memcpy(buffer, MEMORIA_PRINCIPAL + pagina->frame_ppal * CONFIG.tamanio_pagina + offset_header, sizeof(heap_metadata) - diferencia);
		memcpy(buffer+sizeof(heap_metadata) - diferencia, MEMORIA_PRINCIPAL + pagina_sig->frame_ppal * CONFIG.tamanio_pagina, diferencia);
	}

	memcpy(&(header->prev_alloc), buffer + offset, sizeof(uint32_t));
	offset += sizeof(uint32_t);
	memcpy(&(header->next_alloc), buffer + offset, sizeof(uint32_t));
	offset += sizeof(uint32_t);
	memcpy(&(header->is_free), buffer + offset, sizeof(uint8_t));
	return header;
}

void serializar_paginas_en_memoria(t_list* paginas , void* contenido) {

	for (int i = 0 ; i < list_size(paginas); i++) {

		int offset = ((t_pagina*)list_get(paginas , i))->frame_ppal * CONFIG.tamanio_pagina;

		memcpy(MEMORIA_PRINCIPAL + offset, contenido + i * CONFIG.tamanio_pagina, CONFIG.tamanio_pagina);
	}

	return;
}

t_list* obtener_marcos(int cant_marcos) {

	bool marco_libre(t_frame* marco) {
		return marco->ocupado;
	}

	void ocupar_frame(t_frame* marco) {
		marco->ocupado = 1;
	}

	pthread_mutex_lock(&mutexMarcos);
	t_list* marcos_libres = list_filter(MARCOS_MEMORIA, (void*)marco_libre);

	if (list_size(marcos_libres) < cant_marcos) {
		pthread_mutex_unlock(&mutexMarcos);
		return NULL;
	}

	t_list* marcos_asignados = list_take(marcos_libres, cant_marcos);

	for (int i = 0 ; i < list_size(marcos_asignados) ; i++) {
		ocupar_frame((t_frame*)list_take(marcos_asignados , i));
	}
	pthread_mutex_unlock(&mutexMarcos);

	return marcos_asignados;
}

int buscar_pagina_en_memoria(int pid, int pag) {
	t_list* pags_proceso = ((t_tabla_pagina*)list_get(TABLAS_DE_PAGINAS, pid))->paginas;

	if (pags_proceso == NULL) {
		return -1;
	}

	return ((t_pagina*)list_get(pags_proceso, pag))->frame_ppal;
}

void signal_metricas(){
	log_info(LOGGER, "[MEMORIA]: Recibi la senial de imprimir metricas, imprimiendo\n...");
	generar_metricas_tlb();
}
void signal_dump(){
	log_info(LOGGER, "[MEMORIA]: Recibi la senial de generar el dump, generando\n...");
	dumpear_tlb();
}
void signal_clean_tlb(){
	log_info(LOGGER, "[MEMORIA]: Recibi la senial para limpiar TLB, limpiando\n...");
	limpiar_tlb();
}

t_memoria_config crear_archivo_config_memoria(char* ruta) {
    t_config* memoria_config;
    memoria_config = config_create(ruta);
    t_memoria_config config;

    if (memoria_config == NULL) {
        log_info(LOGGER, "No se pudo leer el archivo de configuracion de memoria\n");
        exit(-1);
    }

    config.ip_memoria          = config_get_string_value(memoria_config, "IP");
    config.puerto_memoria      = config_get_string_value(memoria_config, "PUERTO");
    config.tamanio_memoria     = config_get_int_value   (memoria_config, "TAMANIO");
    config.tamanio_pagina      = config_get_int_value   (memoria_config, "TAMANIO_PAGINA");
    config.alg_remp_mmu        = config_get_string_value(memoria_config, "ALGORITMO_REEMPLAZO_MMU");
    config.tipo_asignacion     = config_get_string_value(memoria_config, "TIPO_ASIGNACION");
    config.marcos_max          = config_get_int_value   (memoria_config, "MARCOS_MAXIMOS");  //TODO:--> ver en que casos esta esta esta config y meter un if
    config.cant_entradas_tlb   = config_get_int_value   (memoria_config, "CANTIDAD_ENTRADAS_TLB");
    config.alg_reemplazo_tlb   = config_get_string_value(memoria_config, "ALGORITMO_REEMPLAZO_TLB");
    config.retardo_acierto_tlb = config_get_int_value   (memoria_config, "RETARDO_ACIERTO_TLB");
    config.retardo_fallo_tlb   = config_get_int_value   (memoria_config, "RETARDO_FALLO_TLB");

    return config;
}

void swap(int cantidad_pags) {
	for(int i = 0; i < cantidad_pags; i ++){
		reemplazar_pag_en_memoria();
	}
}

void reemplazar_pag_en_memoria(){
	//reemplazar segun asignacion

	void* pagina_victima; // conseguida despues de correr el algoritmo

	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = SWAP_OUT;

	t_buffer* buffer = malloc(sizeof(t_buffer));
	buffer->size = CONFIG.tamanio_pagina;
	buffer->stream = pagina_victima;

	void* pagina_a_enviar = serializar_paquete(paquete , CONFIG.tamanio_pagina + sizeof(int) * 2);

	//enviar_paquete(paquete , conexion con swap); swap sera servidor?

	eliminar_paquete(paquete);
}
