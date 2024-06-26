#ifndef SERVER_FUNCTIONS_H
#define SERVER_FUNCTIONS_H

#include <stddef.h>
#include "chat.pb-c.h"
#include <stdlib.h>
#include <time.h>
#include "consts.h"


struct Buffer {
    uint8_t *buffer;
    size_t buffer_size;
};

struct User {
    int connection_fd;
    char* name;
    char* ip;
    int status;
    struct User* next_user;
    time_t last_interaction_time;
    bool status_auto_updated;
};

struct User* first_user = NULL;
struct User* last_user = NULL;
int total_users = 0;

/**
 * Función para obtener un usuario registrado en el servidor. 
 * Se busca por name , ip, connection_fd o todos.
 * @param name char*. Nombre del usuario.
 * @param ip char*. ip del usuario.
 * @return struct user. Null si no se encuentra el usuario
*/
struct User* get_user(char*name, char*ip, int* connection_fd){

    struct User* user = first_user;
    while(user != NULL){

        if((name == NULL || strcmp(user->name, name) == 0) 
        && (ip == NULL || strcmp(user->ip, ip) == 0) 
        && (connection_fd == NULL || user->connection_fd == *connection_fd)) {
            return user;
        }
        user = user->next_user;
    }
    return NULL;
}

/**
 * 
 * Función para actualizar automaticamente el estado offline del usuario, dependiendo de la ultima interacción.
 * @param user struct user. Usuario a actualizar.
*/
void auto_update_offline_user_status(struct User *user){

    if(user->status != CHAT__USER_STATUS__ONLINE) return;

    // Tiempo actual
    time_t end_time;
    time(&end_time);

     if (difftime(end_time, user->last_interaction_time) > DOWN_TIME_IN_SECONDS) {
        // Se cumplió tiempo de inactividad, colocarlo como offline
        user->status = CHAT__USER_STATUS__OFFLINE;
        user->status_auto_updated = true; // EL estado fue actualizado auto

    }
}

/**
 * 
 * Función para actualizar automaticamente el estado offline del usuario, dependiendo de la ultima interacción.
 * @param user struct user. Usuario a actualizar.
*/
void auto_update_online_user_status(struct User *user){

    // Si el status no es offline (el cual fue cambiado autom) retornar
    if(user->status != CHAT__USER_STATUS__OFFLINE || !user->status_auto_updated) return;

    // Tiempo actual
    time_t end_time;
    time(&end_time);

     if (difftime(end_time, user->last_interaction_time) < DOWN_TIME_IN_SECONDS) {
        // La inactividad es menor al tiempo establecido, cambiar a activo
        user->status = CHAT__USER_STATUS__ONLINE;
        user->status_auto_updated = true; // EL estado fue actualizado auto

    }
}


/**
 * Función para actualizar la última interacción (reemplazando por el time actual) de un usuario.
 * @param user struct user. Usuario a actualizar.
*/
void update_user_last_interaction(struct User *user){
    time(&user->last_interaction_time);
}

/**
 * Función para registrar un nuevo usuario en el servidor.
 * @param connection_fd int. fd de la conexión por sockets.
 * @param name char*. Nombre del usuario.
 * @param ip char*. ip del usuario.
 * @return String. Error al registrar usuario. NUll si no hay error.
*/
char* register_user(int connection_fd, char* name, char* ip){

    // Verificar que el nombre sea único
    if(get_user(name, NULL, NULL) != NULL){
        return "El nombre de usuario seleccionado no esta disponible.";
    }

    struct User* new_user = malloc(sizeof(struct User));
    new_user->connection_fd = connection_fd;
    new_user->name = name;
    new_user->ip = ip;
    new_user->status = CHAT__USER_STATUS__ONLINE;
    new_user->next_user = NULL;
    new_user->status_auto_updated = false;
    
    // Colocar la última interacción del usuario
    update_user_last_interaction(new_user);

    if(first_user == NULL){
        // Agregar inicio de lista enlazada
        first_user = new_user;
    }

    if(last_user != NULL){
        // Encadenar al final
        last_user->next_user = new_user;
    }

    last_user = new_user;
    total_users += 1;
    return NULL;
}

void print_usernames(){
    
    struct User *curr_user = first_user;
    while(curr_user != NULL){
        printf("%s\n", curr_user->name);
        curr_user = curr_user->next_user;
    }
}

/**
 * Función que devuelve al siguiente usuario en la lista. Si no hay siguiente devuelve null.
 * @param previous_user. struct user*. Puntero al usuario anterior. Si se envía null, se devuelve el primer usuario.
 * @return struct User*.
*/
struct User* get_next_user(struct User *previous_user){

    if(!previous_user) return first_user;
    return previous_user->next_user;
}

struct Buffer get_simple_response(int operation, int status_code, char *message){
    Chat__Response response = CHAT__RESPONSE__INIT;
    response.operation = operation;
    response.status_code = status_code;
    response.message = message;

    size_t size = chat__response__get_packed_size(&response);
	uint8_t *buffer = (uint8_t *)malloc(size);
		
	chat__response__pack(&response, buffer);

    struct Buffer response_buf = {
        buffer,
        size
    };
    return response_buf;

}

/**
 * Función para remover un usuario del servidor.
 * @param connection_fd int. fd de la conexión por sockets.
 * @param strict bool. Si su valor es true, devuelve un error si el usuario no fue encontrado para luego ser removido.
 * @return String. Error al remover usuario. NUll si no hay error.
*/
char* remove_user(int connection_fd, bool strict){

    if(first_user == NULL){
        return strict ? "El usuario no esta registrado.": NULL;
    }

    struct User* user = first_user;
    struct User* prev_user = NULL;
    while(user != NULL){

        if(user->connection_fd == connection_fd){

            // Si el usuario actual es el objetivo
            
            total_users -= 1;

            // Enlazar usuario previo con siguiente
            if(prev_user != NULL){
                prev_user->next_user = user->next_user;
            }

            //ELiminar punteros de inicio y fin de la lista si es un usuario extremo
            if(user == first_user){
                first_user = user->next_user; 
            }else if (user == last_user){
                last_user = prev_user;
            }

            free(user);
            return NULL;
            
        }

        prev_user = user;
        user = user->next_user;
    }

    return strict ? "El usuario no esta registrado.": NULL;
}

/**
 * Función para obtener buffer serializado con la lista de usuarios (o usuario, si se añade filtro).
 * @param username char*. (opcional) Si se agrega, se añade solo la info del usuario especificado. NUll agrega todos los usuarios.
 * @return Buffer. String serializado con objeto respuesta y tamaño del buffer.
*/
struct Buffer get_user_list_response(char *username){
    Chat__Response response = CHAT__RESPONSE__INIT;
    response.operation = CHAT__OPERATION__GET_USERS;
    response.status_code = CHAT__STATUS_CODE__OK;
    response.message = "Lista de usuarios conectados enviada correctamente!";

    int users_to_include = username == NULL ? total_users : 1; // Si se incluye el username, solo se devolverá un usuario

    // Generar lista de usuarios a enviar
    struct Chat__User** users = malloc(users_to_include * sizeof(struct Chat__User*));
    struct User* user = first_user;
    int users_num = 0;
    while(user != NULL && (username == NULL || users_num < 1)){ // Si se incluyó filtro, parar cuando se encuentre el username

        // Tratar de auto actualizar estado offline de cada usuario
        auto_update_offline_user_status(user);

        // Si se filtra por usuario, continuar hasta encontrar al usuario
        if(username != NULL && strcmp(user->name, username) != 0){
            user = user->next_user;
            continue;
        }

        // Guardar puntero de objeto users (protocolo) en lista
        users[users_num] = malloc(sizeof(Chat__User*));

        Chat__User *proto_user = malloc(sizeof(Chat__User));
        chat__user__init(proto_user); // Inicializar el usuario
        proto_user->username = user->name;
        proto_user->status = user->status;

        users[users_num] = proto_user;

        user = user->next_user;
        users_num += 1;
    }

    // Objeto que se envía como parte de la response (size, lista de usuarios(punteros) y tipo)
    Chat__UserListResponse proto_user_list = CHAT__USER_LIST_RESPONSE__INIT;
    proto_user_list.n_users = (size_t) users_num;
    proto_user_list.users = users;
    proto_user_list.type = username == NULL ? CHAT__USER_LIST_TYPE__ALL : CHAT__USER_LIST_TYPE__SINGLE; // tipo: lista completa o solo uno


    // Si se filtra por username y no se encontró mandar error
    if(username != NULL && users_num == 0){
        response.status_code = CHAT__STATUS_CODE__BAD_REQUEST;
        response.message = "El usuario solicitado no existe.";
    }

    response.result_case = CHAT__RESPONSE__RESULT_USER_LIST;
    response.user_list = &proto_user_list;

    size_t size = chat__response__get_packed_size(&response);
	uint8_t *buffer = (uint8_t *)malloc(size);
	
	chat__response__pack(&response, buffer);

    // Liberar memoria
    for(int i = 0; i < users_num; i++){
        free(users[i]);
    }
    free(users);

    struct Buffer response_buf = {
        buffer,
        size
    };
    return response_buf;
}

/**
 * Función para cambiar status de un usuario del servidor.
 * @param connection_fd *int. fd de la conexión por sockets (opcional si se agrega username).
 * @param username *char. Nombre de usuario (opcional si se agrega connection_fd).
 * @param strict bool. Si su valor es true, devuelve un error si el usuario no fue encontrado.
 * @return String. Error al cambiar status de usuario. NUll si no hay error.
*/
char* update_user_status(int *connection_fd, char *username, int status, bool strict){

    if(first_user == NULL || (connection_fd == NULL && username == NULL)){
        return strict ? "No se encontró el usuario.": NULL;
    }

    struct User* user = first_user;
    while(user != NULL){

        if((connection_fd != NULL && user->connection_fd == *connection_fd) || 
            (username != NULL && strcmp(username, user->name) == 0)){
            
            // Usuario encontrado
            user->status = status;
            user->status_auto_updated = false; // EL estado no fue actualizado auto
            return NULL; 
        }
        user = user->next_user;
    }

    return strict ? "No se encontró el usuario.": NULL;
}

struct Buffer get_send_message_response(char* sender, char *message, int type){
    Chat__Response response = CHAT__RESPONSE__INIT;
    response.operation = CHAT__OPERATION__INCOMING_MESSAGE;
    response.status_code = CHAT__STATUS_CODE__OK;
    response.message = "Nuevo mensaje recibido";

    Chat__IncomingMessageResponse message_res = CHAT__INCOMING_MESSAGE_RESPONSE__INIT;
    message_res.content = message;
    message_res.sender = sender;
    message_res.type = type;

    response.incoming_message = &message_res;
    response.result_case = CHAT__RESPONSE__RESULT_INCOMING_MESSAGE;

    size_t size = chat__response__get_packed_size(&response);
	uint8_t *buffer = (uint8_t *)malloc(size);
		
	chat__response__pack(&response, buffer);

    struct Buffer response_buf = {
        buffer,
        size
    };
    return response_buf;

}

#endif