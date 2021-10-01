
#include "tlb.h"

void init_tlb(int entradas , char* algoritmo){

	TLB = list_create();
	TLB = crear_estructura(entradas);

}

t_list* crear_estructura(int cant_entradas){

	t_list* estructuraTLB = list_create();

	for(int i = 0; i < cant_entradas ; i++){

		entrada_tlb* entrada = malloc(sizeof(entrada_tlb));
		entrada->id = i;
		entrada->pag = -1;
		entrada->marco = -1;
		entrada->pid = -1;

		list_add(estructuraTLB , entrada);
	}

	return estructuraTLB;
}

void reemplazar_entrada(entrada_tlb* entrada_nueva, entrada_tlb* entrada_a_reemplazar){

	list_replace_and_destroy_element(TLB , entrada_a_reemplazar->id , (void*)entrada_nueva , (void*)free );
}

void printearTLB(int entradas){
	printf("-------------------\n");
	for(int i=0 ; i< entradas ; i++){
		printf("entrada %i :\n " , i);
		printf("  pag   %i     " , ((entrada_tlb*)list_get(TLB , i))->pag  );
		printf("  marco %i     " , ((entrada_tlb*)list_get(TLB , i))->marco  );
		printf("  pid   %i   \n" , ((entrada_tlb*)list_get(TLB , i))->pid  );
	}
}
