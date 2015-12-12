/*     gcc -c -Wall -Wshadow -g shell.c
		
		PRACTICA FINAL

            gcc -o shell  shell.o  */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <err.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>

typedef struct  {
	char* comando;
	char* args[128];
	int nargs;
}comand;

typedef struct 
{
	comand comandos[30];
	int contador;
	int background;
	char* fichin;
	char* fichout;
}pipecomand;


void
executehijo(pipecomand c,char** paths);

void
closepipes(int pipes[40][2],int npipes);

void
executepipehijo(pipecomand c,char** paths,int pipes[40][2],int i);

void
tokpath(char*** path);


int
firstfichout(char* linea);

void
tokcomandos(pipecomand* c);

int
esbuiltin(pipecomand c, char* home);

int
procesarsetenv(char* linea,char* prompt);

void
procesargetenv(pipecomand* c,int* hayerror);

int
tokenize(char* strg,char*** varaux,char* sep);

void
redirigirfichout(char* fichout);

void
redirigirfichin(char* fichin);

char*
tokstr(char* strg);

void
tokpipe(char* cmd,pipecomand* c);

char*
concatenarpath(char* path, char* fichname);

char*
pathaccess(char** paths, char* cmd);

void
executecmd(pipecomand c,char** paths);

int
main(int argc, char* argv[]){
	
	char prompt[128];
	char buffer[1024];
	char** paths;
	char** varaux;
	char* linea = NULL;
	pipecomand c;
	char* home = NULL;
	int hayerror = 0;
	int ntoken = 0;

	paths = (char**) calloc(sizeof(char*), 40);
	varaux = (char**) calloc(sizeof(char*), 2);
	home = getenv("HOME");
	strcpy(prompt,"linux-shell$");
	tokpath(&paths);
	for(;;){

		c.fichout = NULL;
		c.fichin = NULL;
		hayerror = 0;
		ntoken = 0;
		c.background = 0;

		if(isatty(0)){
			printf("%s ",prompt);
		}
		
		linea = fgets(buffer,sizeof(buffer),stdin);
		

		if(linea == NULL){
			break;
		}	


		if(procesarsetenv(linea,prompt)){
			continue;
		}

		ntoken = tokenize(linea,&varaux,"&");
		if(ntoken > 1){
		 	c.background = 1;
		 }
		
		if(firstfichout(varaux[0])) {
			ntoken = tokenize(varaux[0],&varaux,"<");
		 	if(ntoken > 1){
		 		c.fichin = tokstr(varaux[1]);
		 	}
			ntoken = tokenize(varaux[0],&varaux,">");
			if(ntoken > 1){
				c.fichout = tokstr(varaux[1]);
		 	}
		}else{
			ntoken = tokenize(varaux[0],&varaux,">");
		 	if(ntoken > 1){
		 		c.fichout = tokstr(varaux[1]);
		 	}
			ntoken = tokenize(varaux[0],&varaux,"<");
			if(ntoken > 1){
				c.fichin = tokstr(varaux[1]);
		 	}
		}

		tokpipe(varaux[0],&c);
		tokcomandos(&c);


		if(c.comandos[0].nargs == 0 || c.contador == 0){
			continue;
		}

		if(strcmp(c.comandos[0].args[0],"exit") ==0){
			break;
		}

		procesargetenv(&c, &hayerror);
		if(hayerror){
			continue;
		}	

		if(esbuiltin(c,home)){
			continue;
		}

		executecmd(c,paths);

	}
	free(varaux);
	free(paths[1]);
	free(paths);
	exit(0);
}


int
firstfichout(char* linea){
	int findfichout  = 0;
	int findfichin = 0;
	int i;
	for(i = 0; i< strlen(linea);i++){
		if(linea[i] == '>'){
			findfichout = i;
		}
	}
	for(i = 0; i< strlen(linea);i++){
		if(linea[i] == '<'){
			findfichin = i;
		}
	}

	return (findfichout < findfichin);
}



void
tokpath(char*** ppaths){
	char* sep = ":";
	char* path = NULL;
	char* resto;
	char* token = NULL;
	int i = 0;


	path = getenv("PATH");
	path = strdup(path);
	(*ppaths)[i] = "."; //Mi primera posicion del array es el punto para buscar en el directorio de trabajo
	for(token = strtok_r(path, sep, &resto);token;
			token = strtok_r(NULL, sep, &resto)){
		i++;
		(*ppaths)[i] = token;
	}


}



int
procesarsetenv(char* linea,char* prompt){
	char* valor = NULL;
	char* valoraux = NULL;
	char* variable = NULL;
	char* sep = "\n\t= ";
	char* resto;

	if(strrchr(linea,'=') == NULL){
		return 0;
	}

	variable= strtok_r(linea, sep, &resto);
	if(variable == NULL){
		return 0;
	}
	valor=  strtok_r(NULL, sep, &resto);
	if(valor == NULL){
		return 0;
	}

	if(valor[0] =='$'){
		valoraux = valor;
		valoraux = valoraux +1;
		valor = getenv (valoraux);
		if(valor == NULL){
			fprintf(stderr, "error: var %s does not exist \n",valoraux);
			return 1;
		}
	}

	if(strcmp(variable,"PS") == 0){
		strcpy(prompt,valor);
		return 1;
	}

	setenv(variable,valor,1);
	return 1;

}


void
procesargetenv(pipecomand* c,int* hayerror){
	int i,j;
	char* valoraux = NULL;
	char* valor = NULL;

	for (j = 0; j< c->contador;j++){
		for(i = 0;c->comandos[j].args[i];i++){
			if(c->comandos[j].args[i][0] =='$'){
				valoraux =c->comandos[j].args[i];
				valoraux = valoraux+1;
				valor = getenv (valoraux);
				if(valor == NULL){
					fprintf(stderr, "error: var %s does not exist \n",valoraux);
					*hayerror = 1;
				}
				c->comandos[j].args[i] = valor;

			}
		}
	}
	
	if(c->fichin != NULL){
		if(c->fichin[0]=='$'){
			valoraux = c->fichin;
			valoraux = valoraux+1;
			valor = getenv (valoraux);
			if(valor == NULL){
				fprintf(stderr, "error: var %s does not exist \n",valoraux);
				*hayerror = 1;

			}
			c->fichin = valor;
		}
	}

	if(c->fichout != NULL){
		if(c->fichout[0]=='$'){
			valoraux = c->fichout;
			valoraux = valoraux+1;
			valor = getenv (valoraux);
			if(valor == NULL){
				fprintf(stderr, "error: var %s does not exist \n",valoraux);
				*hayerror = 1;
			}
			c->fichout = valor;
		}
	}


}

int
esbuiltin(pipecomand c, char* home){
	int n;


	
	if(strcmp(c.comandos[0].args[0],"cd") == 0){
		if(c.comandos[0].args[1] == NULL){
			n = chdir(home);
			if(n<0){
				fprintf(stderr, "ch dir no es correcto el directorio %s\n",home);
			}
			return 1;
		}else{
			n = chdir(c.comandos[0].args[1]);
			if(n<0){
				fprintf(stderr, "ch dir no es correcto el directorio %s\n",c.comandos[0].args[1]);
			}
			return 1;
		}
	}
	return 0;
	


}
void
closepipes(int pipes[40][2],int npipes){
	int j;

	for(j=0; j < npipes; j++){
		close(pipes[j][0]);
		close(pipes[j][1]);
	}

}

void
redirigirfichin(char* fichin){
	int fd = 0;

	fd = open (fichin,O_RDONLY);
		if(fd < 0){
			err(1,"open 1 fichin ");
		}
	dup2(fd,0);
	close(fd);


}

void
redirigirfichout(char* fichout){
	int fd = 0;

	fd= open(fichout,O_WRONLY|O_TRUNC|O_CREAT, 0660);
		if(fd < 0){
			err(1,"open 1 fichout");
	}
	dup2(fd,1);
	close(fd);

	
}

void
executehijo(pipecomand c,char** paths){
	char* pathaux = NULL;
	int n  = 0;


	if(c.fichin != NULL){
		redirigirfichin(c.fichin);
	}else if(c.background && c.fichin == NULL){
		redirigirfichin("/dev/null");
	
	}
	if(c.fichout != NULL){
		redirigirfichout(c.fichout);
	}
	pathaux = pathaccess(paths,c.comandos[c.contador - 1].args[0]);
	n = execv(pathaux,c.comandos[c.contador - 1].args);		// No hago el free de pathaux porque el execv ya lo hace
	if(n < 0){
		err(1,"Execv");
	}
		



}

char*
pathaccess(char** paths, char* cmd){
	char* pathaux = NULL;
	int i,vaccess;

	for(i = 0; paths[i]; i++){
		pathaux = concatenarpath(paths[i],cmd);
		vaccess = access(pathaux,X_OK);
		if(vaccess == 0){
			break;
		}
	}

	if(vaccess != 0){
		free(pathaux);
		err(1,"No existe comando %s",cmd);
	}

	return pathaux;
}

void
executecmd(pipecomand c, char** paths){
	int i = 0;
	int pid_hijo = 0;
	int pid = 0;
	int pipes[40][2];
	int npipes = c.contador-1;

	if(c.contador > 1){
		for(i=0; i < npipes; i++){
				pipe(pipes[i]);
		}
	}

	for(i = 0; i < c.contador; i++){
		pid_hijo = fork();
		switch(pid_hijo){
		case -1:
			err(1, "fork failed");
			break;
		case 0:	
			if(c.contador == 1){
				executehijo(c,paths);
			}else if(c.contador > 1){
				 executepipehijo(c,paths,pipes,i);
			}
		}
	}

	closepipes(pipes,npipes);
	if(!c.background){
		while ( pid_hijo != pid ) {
			pid= wait(NULL);
		}
	}
	
	
}

void
executepipehijo(pipecomand c,char** paths,int pipes[40][2],int i){
	char* pathaux = NULL;
	int npipes = c.contador-1;
	int n = 0;

	if (i == 0){
		if(c.fichin != NULL){
			redirigirfichin(c.fichin);
		}else if(c.background && c.fichin == NULL){
			redirigirfichin("/dev/null");
		}
		dup2(pipes[i][1],1); // Cambio la salida estandar al pipe de write
	} else if(i > 0 && i != (c.contador -1)){
		dup2(pipes[i-1][0],0); // Cambio la entrada estandar al pipe de read
		dup2(pipes[i][1],1); // cambio la salida estandar al pipe de write 
	}else if(i == (c.contador-1)) {	
		if(c.fichout != NULL){
			redirigirfichout(c.fichout);
		}
		dup2(pipes[i-1][0],0); // cambio la entrada estandar al pipe de read 
	}
	closepipes(pipes,npipes);
	pathaux = pathaccess(paths,c.comandos[i].args[0]);
	n = execv(pathaux,c.comandos[i].args);	// No hago el free de pathaux porque el execv ya lo hace
	if(n < 0){
		err(1,"Execv");
	}
}

int
tokenize(char* strg, char*** varaux,char* sep){
	char* resto;
	char* token;
	int i =0;


	for(token = strtok_r(strg, sep, &resto);token;
			token = strtok_r(NULL, sep, &resto)){
		(*varaux)[i] = token;
		i++;
	}
	return i;
}


char*
tokstr(char* cmd){
	char* sep = "\t \n\r";
	char* resto;
	char* token;


	for(token = strtok_r(cmd, sep, &resto);token;
			token = strtok_r(NULL, sep, &resto)){
			return token;
	}
	return token;
}


void
tokpipe(char* cmd,pipecomand* c){

	char* sep = "|\n\r";
	char* resto;
	char* token;
	int i =0;



	for(token = strtok_r(cmd, sep, &resto);token;
			token = strtok_r(NULL, sep, &resto)){
			c->comandos[i].args[0] = token;
			i++;
	}
	c->contador = i;
	
}

void
tokcomandos(pipecomand* c){
	char* sep = "\t \n\r";
	char* resto;
	char* token;
	int i =0;
	int j = 0;
	
	for(j=0;j<c->contador;j++){
		for(token = strtok_r(c->comandos[j].args[0], sep, &resto);token;
				token = strtok_r(NULL, sep, &resto)){
				c->comandos[j].args[i] = token;
				i++;
		}
		c->comandos[j].nargs = i;
		c->comandos[j].comando =c->comandos[j].args[0];
		c->comandos[j].args[i] = NULL;  //condicion de salida para bucles
		i =0;   //Para volver a guardar en args desde 0
	}

}

char*
concatenarpath(char* path, char* fichname){
	int tam = 0;
	char* pathaux = NULL;
	char slash[] = "/";
	
	tam = strlen(path);
	tam++; //Le sumo el tamaño del '/'
	tam += strlen(fichname);
	tam++; //Para almacenar el \0
	
	pathaux = (char*) malloc(tam * sizeof (char));

	strcpy(pathaux, path);
	strcat(pathaux,slash);
	strcat(pathaux,fichname);

	pathaux[tam - 1] = '\0';

	return pathaux;
}
