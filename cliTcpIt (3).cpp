#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <iostream>
#include <sqlite3.h>
#include <wait.h>
#include <ctype.h>
#include <stdexcept>

using namespace std;

extern int errno;
int port;

void clearMsg(char msg[]);

string getDownLink(char msg2[]);

void verifyOption(char msg[], int nrOpt);

void getValidRating(char msg1[]);

void logare(char msg[], int sd);

void downloadBook(string d);

void getSearchStr(char msg[], char msg2[]);

int main (int argc, char *argv[])
{
    int sd;
    struct sockaddr_in server;
    char msg[200];
    char msg1[200];
    char msg2[300];
    if (argc != 3)
    {
        printf ("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
        return -1;
    }
    port = atoi (argv[2]);

    if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror ("Eroare la socket().\n");
        return errno;
    }
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_port = htons (port);
    if (connect (sd, (struct sockaddr *) &server,sizeof (struct sockaddr)) == -1)
    {
        perror ("[client]Eroare la connect().\n");
        return errno;
    }
    bzero (msg, 200);
    fflush (stdout);

    if (read (sd, msg, 200) < 0)
    {
        perror ("[client]Eroare la read() de la server.\n");
        return errno;
    }
    ///A primit primul mesaj de la server
    printf ("%s", msg);
    clearMsg(msg);
    verifyOption(msg,2);
    msg[strlen(msg)-1]='\0';
    if (write (sd, msg, 200) <= 0)
    {
        perror ("[client]Eroare la write() spre server.\n");
        return errno;
    }
    logare(msg,sd);
    clearMsg(msg);
    while(true){
        printf("\033c");
        meniu:
        int ok=1, s;
        clearMsg(msg);
        cout<<"Meniu:\n 1 Search\n 2 Recomandari\n 3 Ierarhia\n 4 Autori\n 5 Quit\n";
        verifyOption(msg,5);
        char books[6000];
        clearMsg(books);
        if(msg[0]=='1')
        {
            getSearchStr(msg,msg2);
            write (sd, msg, 200);
            read (sd, books, 6000);
            s = strlen(books);
            if(s+s>4){
                inceput_lista:
                printf("\033c");
                cout<<books;
                clearMsg(msg1);
                cout<<"Optiuni disponibile:"<<endl<<" 1. Detalii carte"<<endl<<" 2. Inapoi la meniu"<<endl<<" 3. Logout"<<endl;
                verifyOption(msg1,3);
                if(msg1[0]=='1'){
                    ///clientul a deschis cartea
                    cout<<"\nSpecificati ID-ul corespunzator cartii: ";
                    clearMsg(msg2);
                    read (0, msg2, 200);
                    msg1[strlen(msg1)-1]='\0';
                    msg2[strlen(msg2)-1]='\0';
                    strcat(msg1,msg2);
                    write (sd, msg1, 200);
                    clearMsg(msg2);
                    read (sd, msg2, 300);
                    s = strlen(msg2);
                    if(s+s>4){
                        ///aici se intra doar daca am gasit cartea dupa id-ul ala
                        printf("\033c");
                        cout<<msg2;
                        cout<<"Optiuni disponibile:"<<endl<<" 1. Rate"<<endl<<" 2. Download"<<endl<<" 3. Inapoi la Lista"<<endl<<" 4. Inapoi la meniu"<<endl<<" 5. Logout"<<endl;
                        clearMsg(msg1);
                        verifyOption(msg1,5);
                        write(sd,msg1,3);
                        ///cazurile 1 2 3
                        if(msg1[0]=='5'){
                            write(sd,"5",2);
                            break;
                        }
                        if(msg1[0]=='2'){
                            ///download la carte
                            string downLink = getDownLink(msg2);
                            downloadBook(downLink);
                        }
                        else if(msg1[0]=='1'){
                            ///rate cartii
                            cout<<endl<<"Rating(0 - 5): ";
                            getValidRating(msg1);
                            write(sd,msg1,4);
                        }
                        else if(msg1[0]=='3'){
                            ///inapoi la lista
                            goto inceput_lista;
                        }
                    }
                    else{
                        cout<<endl<<"Nu exista nicio carte cu ID-ul specificat!";
                        ok=0;
                    }
                }
                else if(msg1[0]=='2'){
                    ///inapoi la meniu
                    write (sd, "2", 2);
                    clearMsg(msg2);
                }
                else if(msg1[0]=='3'){
                    write (sd, "3", 2);
                    break;
                }
            }
            else{
                cout<<endl<<"Nu s-a gasit nicio carte";
                ok=0;
            }
        }
        else if(msg[0]=='2'){
            write(sd,"2",2);
            read(sd,books,2000);
            printf("\033c");
            cout<<books<<endl<<endl;
            goto inceput_lista;
            ///recomandari
        }
        else if(msg[0]=='3'){
            printf("\033c");
            write(sd,"3",2);
            ///ierarhia
            char ierarhie[400];
            bzero(ierarhie,400);
            read(sd,ierarhie,400);
            cout<<ierarhie<<endl<<endl;
            clearMsg(ierarhie);
            clearMsg(msg1);
            goto meniu;
        }
        else if(msg[0]=='4'){
            printf("\033c");
            write(sd,"4",2);
            fflush(stdout);
            ///preferintele autorilor
            char autori[2500];
            bzero(autori,2500);
            read(sd,autori,2500);
            cout<<autori<<endl<<endl;
            clearMsg(autori);
            goto meniu;
        }
        else if(msg[0]=='5'){
            write(sd,"5",2);
            break;
        }
        clearMsg(msg1);
        clearMsg(msg2);
        if(ok==0)
            sleep(2);
    }
    close (sd);
}

void clearMsg(char msg[]){
    bzero (msg, 200);
    fflush (stdout);
}

string getDownLink(char msg2[]){
    char *down1;
    down1 = strtok(msg2,":");
    down1 = strtok(NULL,":");
    down1 = strtok(NULL,":");
    down1 = strtok(NULL,":");
    down1 = strtok(NULL,":");
    down1 = strtok(NULL,":");
    down1 = strtok(NULL,":");
    down1 = strtok(NULL,":");
    down1 = strtok(NULL,":");
    down1 = strtok(NULL,":");
    string downLink = "http:";
    downLink += down1;
    return downLink;
}

void verifyOption(char msg[], int nrOpt){
    int ok=0;
    while(true){
        clearMsg(msg);
        read (0, msg, 200);
        msg[strlen(msg)-1]='\0';
        for(int i = 1; i<=nrOpt; i++)
            if(string(msg)==to_string(i))
                ok=1;
        if(ok==0)
            cout<<"\nAlegeti una din optiunile de mai sus!\n";
        else
            break;
    }
    msg[strlen(msg)]='\n';
}

void getValidRating(char msg1[]){
    while(true){
        clearMsg(msg1);
        read(0,msg1,4);
        msg1[strlen(msg1)-1]='\0';
        try{
            float x = stof(msg1);
            if(x>0 && x<=5)
                break;
            cout<<"Nu ati intoduc un numar valid! Numarul trebuie sa fie intre 0 si 5."<<endl;
        }
        catch(const invalid_argument& ia){
            cout<<"Nu ati intoduc un numar valid!"<<endl;
        }
    }
}

void logare(char msg[], int sd){
    while(true){
        clearMsg(msg);
        cout<<"Introduceti numele de utilizator: ";
        cin>>msg;
        write (sd, msg, 200);
        clearMsg(msg);
        cout<<"Introduceti parola: ";
        cin>>msg;
        write (sd, msg, 200);
        clearMsg(msg);
        read (sd, msg, 200);
        printf("%s\n",msg);
        if(msg[0]=='C')
            break;
    }
}

void downloadBook(string d){
    pid_t pid;
    int status;
    char str[180] = "";
    strcpy(str,d.c_str());
    str[strlen(str)-2]='\0';
    if((pid=fork())==0){
        execlp("wget","wget",str,NULL);
        exit(1);
    }
    wait(&status);
}

void getSearchStr(char msg[], char msg2[]){
    cout<<"\nSelectati criteriul dupa care doriti sa faceti cautarea: \n 1 Gen \n 2 Autor \n 3 Titlu \n 4 An aparitie \n 5 ISBN \n 6 Rating \n";
    clearMsg(msg2);
    verifyOption(msg2,6);
    strcat(msg,msg2);
    if(msg2[0]=='1') cout<<"\nSpecificati genul: ";
    else if(msg2[0]=='2') cout<<"\nSpecificati autorul: ";
    else if(msg2[0]=='3') cout<<"\nSpecificati titlul: ";
    else if(msg2[0]=='4') cout<<"\nSpecificati anul aparitiei: ";
    else if(msg2[0]=='5') cout<<"\nSpecificati ISBN: ";
    else if(msg2[0]=='6') cout<<"\nSpecificati ratingul: ";
    clearMsg(msg2);
    read (0, msg2, 200);
    strcat(msg,msg2);
    msg[strlen(msg)-1]='\0';
}
