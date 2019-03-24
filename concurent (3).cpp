#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <wait.h>
#include <iostream>
#include <sqlite3.h>
#include <vector>
#include <string>
#include <algorithm>

/* portul folosit */
#define PORT 2025

using namespace std;
using Books = vector<string>;
using Users = vector<string>;

extern int errno;

void sighandler(int sig);

void clearMsg(char msg[]);

void BookToChar(Books book, char msg[]);

void BooksToChar(Books bookList, char searchedBooks[]);

bool verifyUsername(char m[], Users users);

bool verifyUser(char name[], char pass[], Users users);

static int userList(void *unused, int countt, char **data, char **columns);

static int getAllData(void *unused, int countt, char **data, char **columns);

void getAuthToChar(vector<string> genuri,vector<string> authorsList, char autoric[]);

float getNewRating(vector<string> rating);

string getSQLSearchCommand (char msg[]);

void verificareAccesat(sqlite3* db, int id_user, string id_carte);

void verificareDownload(sqlite3* db, int id_user, string id_carte);

void verificareCautare(sqlite3* db, int id_user, char cautare[], char tip);

int ratingOption(sqlite3* db, char msgrasp[], int id_user, string id, int client);

void logare(vector<string> usersList, int client, sqlite3* db, char msg[], char msgrasp[]);

void updatePreferinteGenuri(sqlite3* db,int id_user);

void recomandari(sqlite3* db, int id_user, Books *recomandations);

void RecomandationsToChar(Books bookList, char searchedBooks[], sqlite3* db);

int main ()
{
    struct sockaddr_in server;
    struct sockaddr_in from;
    char msg[200];
    char msgrasp[200]=" ";
    char msgrasp1[200]=" ";
    int sd;
    int child;
    sqlite3 *db;

    //Deschiderea bazei de date
    if(sqlite3_open("bazacarti.db", &db))
        cout<<"Nu am putut deschide baza de date";
    /* crearea unui socket */
    if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror ("[server]Eroare la socket().\n");
        return errno;
    }
    bzero (&server, sizeof (server));
    bzero (&from, sizeof (from));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl (INADDR_ANY);
    server.sin_port = htons (PORT);
    if (bind (sd, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1)
    {
        perror ("[server]Eroare la bind().\n");
        return errno;
    }
    if(signal(SIGCHLD,sighandler) == SIG_ERR)
        printf("It's ok");
    if (listen (sd, 5) == -1)
    {
        perror ("[server]Eroare la listen().\n");
        return errno;
    }

    while (true)
    {
        int client, rc;
        socklen_t length = sizeof (from);
        printf ("[server]Asteptam la portul %d...\n",PORT);
        fflush (stdout);
        client = accept (sd, (struct sockaddr *) &from, &length);
        if ((child = fork()) == -1)
            perror("Err...fork");
        else
            if (child==0)   //copil
            {
                if (client < 0){
                    perror ("[server]Eroare la accept().\n");
                    continue;
                }
                string sqlCommand;
                bzero(msgrasp,200);
                strcat(msgrasp,"Welcome to ReadsProfiler! Choose one of the following:\n 1 Login\n 2 Create an account\n");
                if (write (client, msgrasp, 200) <= 0){
                    perror ("[server]Eroare la write() catre client.\n");
                    continue;
                }
                else
                    printf ("[server]Mesaj trasmis cu succes.\n");
                clearMsg(msg);
                ///Primesc 1 pentru login sau 2 pentru crearea contului
                if (read (client, msg, 100) <= 0){
                    perror ("[server]Eroare la read() de la client.\n");
                    close (client);
                    continue;
                }
                ///Logare sau creare client cu username/parola
                Users usersList;
                int rc = sqlite3_exec(db,"select * from user;",userList,&usersList,NULL);
                logare(usersList, client, db, msg, msgrasp);
                ///clientul s-a logat cu succes
                cout<<"S-a logat un client: "<<msgrasp<<"\n";
                usersList.clear();
                sqlCommand = "select id from user where username='" + string(msgrasp) + "';";
                rc = sqlite3_exec(db,sqlCommand.c_str(),getAllData,&usersList,NULL);
                ///ii salvez id-ul pentru viitoarele insert-uri
                int id_user=atoi(usersList[0].c_str());
                int cap;
                bzero(msgrasp,200);
                bzero(msgrasp1,200);
                while(true){
                    inceput_meniu:
                    Books bookList;
                    ///Prima selectie a clientului
                    ///1 Search         2 Recomandari       3 Ierarhia genurilor       4 Autori       5 Quit
                    clearMsg(msg);
                    string sql;
                    char searchedBooks[6000]="";
                    read (client, msg, 200);
                    if(msg[4]>90 && msg[0]=='1')
                        msg[4]-=32;
                    if(msg[0]=='1'){
                        ///se efectueaza o cautare
                        sql = getSQLSearchCommand(msg);
                        verificareCautare(db,id_user,msg, msg[2]);
                        //cout<<endl<<sql<<endl;
                        rc = sqlite3_exec(db,sql.c_str(),getAllData,&bookList,NULL);
                        bookList.shrink_to_fit();
                        cap = bookList.capacity();
                        if(cap>0){
                            ///s-au gasit carti conform cautarii
                            BooksToChar(bookList,searchedBooks);
                            write(client, searchedBooks, 6000);
                        }
                        else
                            write(client, "00", 6000);
                        if(cap>0){
                            ///A doua selectie a clientului
                            ///1 Detalii carte         2 Inapoi la meniu       3 Quit
                            lista_carti:
                            clearMsg(msg);
                            read (client, msg, 200);
                            if(msg[0]=='1'){
                                ///clientul a deschis cartea
                                string id = string(msg+1);
                                Books book;
                                sqlCommand = "select * from carte c join autor a on c.id_autor=a.id where c.id=" + id + ";";
                                //cout<<sqlCommand<<endl;
                                rc = sqlite3_exec(db,sqlCommand.c_str(),getAllData,&book,NULL);
                                sqlCommand = "select g.nume from gen g join gencarte gc on gc.id_gen=g.id join carte c on c.id=gc.id_carte where c.id=" + id + ";";
                                //cout<<sqlCommand<<endl;
                                rc = sqlite3_exec(db,sqlCommand.c_str(),getAllData,&book,NULL);
                                clearMsg(msg);
                                cap = book.capacity();
                                if(cap>0){
                                    ///id-ul primit corespunde unei carti
                                    BookToChar(book,msg);
                                    write(client, msg, 300);
                                }
                                else
                                    write(client, "00", 6000);
                                if(cap>0){
                                    ///A treia selectie a clientului
                                    ///1 Rate         2 Download         3 Inapoi la lista         4 Inapoi la meniu       5 Quit
                                    verificareAccesat(db,id_user,id);
                                    clearMsg(msgrasp1);
                                    read(client,msgrasp1,3);
                                    if(msgrasp1[0]=='2'){
                                        ///clientul a downloadat cartea
                                        verificareDownload(db,id_user,id);
                                        updatePreferinteGenuri(db,id_user);
                                    }
                                    else if(msgrasp1[0]=='1'){
                                        if(ratingOption(db, msgrasp, id_user, id, client)==0)
                                            break;
                                    }
                                    else if(msgrasp1[0]=='3'){
                                        ///clientul vrea sa dea mearga inapoi la lista
                                        goto lista_carti;
                                    }
                                    else if(msgrasp1[0]=='5')
                                        break;
                                }
                            }
                            else if(msg[0]=='2'){
                                ///merge inapoi la meniu
                                clearMsg(msg);
                            }
                            else if(msg[0]=='3')
                                break;
                        }///daca nu intra in if inseamna ca nu am gasit nicio carte dupa cautarile lui si il trimit inapoi la meniu
                    }
                    else if(msg[0]=='2'){
                        Books bookList;
                        recomandari(db,id_user,&bookList);
                        int sizer = bookList.size();
                        char recom[2000];
                        bzero(recom,2000);
                        RecomandationsToChar(bookList, recom, db);
                        write(client, recom, 2000);
                        goto lista_carti;
                        ///recomandari
                    }
                    else if(msg[0]=='3'){
                        ///ierarhia
                        char ierarhia[400];
                        bzero(ierarhia,400);
                        strcat(ierarhia,"|--FICTIUNE\n|         |--CLASIC\n|         |--POLITIST\n|         |--DRAGOSTE\n|         |--MISTER\n|         |--FANTASTIC\n");
                        strcat(ierarhia,"|         |--LIRIC\n|                `--POEZIE\n|         |--TEATRU\n|         |       |--COMEDIE\n|                 `--DRAMA\n");
                        strcat(ierarhia,"|         |--HORROR\n|         `--BALADA\n|--NONFICTIUNE\n             |--STIINTIFIC\n             `--PSIHOLOGIE\n\n");
                        write(client,ierarhia,400);
                    }
                    else if(msg[0]=='4'){
                        ///preferintele autorilor
                        char autoric[2500];
                        bzero(autoric,2500);
                        vector<string> authorsList, genuri;
                        ///lista cu autori + genuri si o lista cu genuri
                        sqlCommand = "select distinct a.nume, g.nume from carte c join autor a on c.id_autor=a.id join gencarte gc on c.id=gc.id_carte join gen g on g.id=gc.id_gen;";
                        rc = sqlite3_exec(db,sqlCommand.c_str(),getAllData,&authorsList,NULL);
                        rc = sqlite3_exec(db,"select nume from gen;",getAllData,&genuri,NULL);
                        getAuthToChar(genuri,authorsList,autoric);
                        write(client,autoric,2500);
                    }
                    else if(msg[0]=='5')
                        break;
                    clearMsg(msg);
                }
                sqlite3_close(db);
                close (client);
                cout<<endl<<"A iesit clientul cu id-ul "<<id_user;
                exit(0);
            }
    }
}

void clearMsg(char msg[]){
    bzero (msg, 200);
    fflush (stdout);
}

void sighandler(int sig){
	if(sig==SIGCHLD)
		while(waitpid(-1,0,WNOHANG) > 0) { }
}

void BookToChar(Books book, char msg[]){
    string s;
    strcat(msg,"ID: "); s = book[0]; strcat(msg,s.c_str()); strcat(msg,"\n");
    strcat(msg,"Titlu: "); s = book[1]; strcat(msg,s.c_str()); strcat(msg,"\n");
    strcat(msg,"Gen: ");
    int sizee = book.size();
    for(auto i=9;i!=sizee;i++){
        s = book[i]; strcat(msg,s.c_str()); strcat(msg,"  ");
    }
    strcat(msg,"\n");
    strcat(msg,"Anul aparitiei: "); s = book[2]; strcat(msg,s.c_str()); strcat(msg,"\n");
    strcat(msg,"ISBN: "); s = book[3]; strcat(msg,s.c_str()); strcat(msg,"\n");
    strcat(msg,"Rating: "); s = book[4]; strcat(msg,s.c_str()); strcat(msg,"\n");
    strcat(msg,"Autor: "); s = book[8]; strcat(msg,s.c_str()); strcat(msg,"\n");
    strcat(msg,"Download link: "); s = book[5]; strcat(msg,s.c_str()); strcat(msg,"\n\n");
}

void BooksToChar(Books bookList, char searchedBooks[]){
    for(auto i=0; i<bookList.size(); i=i+6){
        string s;
        strcat(searchedBooks,"ID: "); s = bookList[i]; strcat(searchedBooks,s.c_str()); strcat(searchedBooks,"\n");
        strcat(searchedBooks,"Titlu: "); s = bookList[i+1]; strcat(searchedBooks,s.c_str()); strcat(searchedBooks,"\n");
        strcat(searchedBooks,"Autor: "); s = bookList[i+2]; strcat(searchedBooks,s.c_str()); strcat(searchedBooks,"\n");
        strcat(searchedBooks,"Anul aparitiei: "); s = bookList[i+3]; strcat(searchedBooks,s.c_str()); strcat(searchedBooks,"\n");
        strcat(searchedBooks,"ISBN: "); s = bookList[i+4]; strcat(searchedBooks,s.c_str()); strcat(searchedBooks,"\n");
        strcat(searchedBooks,"Rating: "); s = bookList[i+5]; strcat(searchedBooks,s.c_str()); strcat(searchedBooks,"\n\n");
    }
}

void RecomandationsToChar(Books bookList, char searchedBooks[], sqlite3* db){
    int sizee = bookList.size();
    int rc;
    vector<string> genuri;
    for(auto i=0; i<sizee; i=i+4){
        string s;
        strcat(searchedBooks,"ID: "); s = bookList[i]; strcat(searchedBooks,s.c_str()); strcat(searchedBooks,"\t");
        strcat(searchedBooks,"Titlu: "); s = bookList[i+1]; strcat(searchedBooks,s.c_str()); strcat(searchedBooks,"\n");
        strcat(searchedBooks,"Autor: "); s = bookList[i+2]; strcat(searchedBooks,s.c_str()); strcat(searchedBooks,"\t");
        strcat(searchedBooks,"Anul aparitiei: "); s = bookList[i+3]; strcat(searchedBooks,s.c_str()); strcat(searchedBooks,"\n");
        strcat(searchedBooks,"Gen: ");
        s = "select g.nume from carte c join gencarte gc on c.id=gc.id_carte join gen g on g.id=gc.id_gen where c.id=" + bookList[i] + ";";
        rc = sqlite3_exec(db,s.c_str(),getAllData,&genuri,NULL);
        int sizeG = genuri.size();
        for(int j=0; j<sizeG; j++){
            s = genuri[j];
            strcat(searchedBooks,s.c_str());
            if(j<sizeG-1)
                strcat(searchedBooks,", ");
        }
        genuri.clear();
        strcat(searchedBooks,"\n\n");
    }
}

bool verifyUsername(char m[], Users users){
    for(int i=0; i<users.size(); i+=2)
        if(m==users[i])
            return true;
    return false;
}

bool verifyUser(char name[], char pass[], Users users){
    for(int i=0; i<users.size(); i+=2)
        if(name==users[i] && pass==users[i+1])
            return true;
    return false;
}

static int userList(void *unused, int countt, char **data, char **columns) {
    Users* usersList = static_cast<Users*>(unused);
    for(int i=1, j=i+1; i<countt; i+=3){
        usersList->push_back(data[i]);
        usersList->push_back(data[j]);
    }
    return 0;
}

static int getAllData(void *unused, int countt, char **data, char **columns) {
    Users* usersList = static_cast<Users*>(unused);
    for(int i=0; i<countt; i++){
        usersList->push_back(data[i]);
    }
    return 0;
}

void getAuthToChar(vector<string> genuri,vector<string> authorsList, char autoric[]){
    vector<string> autoriLuati;
    for(int i=0;i<authorsList.size();i++){
        if(find(genuri.begin(),genuri.end(),authorsList[i])!=genuri.end()){
            ///e un gen
            strcat(autoric,authorsList[i].c_str());
            strcat(autoric," ");
        }
        else{
            ///nu e un gen
            if(find(autoriLuati.begin(),autoriLuati.end(),authorsList[i])==autoriLuati.end()){
                strcat(autoric,"\n");
                strcat(autoric,authorsList[i].c_str());
                strcat(autoric," : ");
                autoriLuati.push_back(authorsList[i]);
            }
        }
    }
}

float getNewRating(vector<string> rating){
    float r;
    float rsum = 0;
    for(auto i : rating){
        rsum += stof(i);
    }
    r = rsum/rating.size();
    return r;
}

string getSQLSearchCommand (char msg[]){
    string sql = "select c.id, c.titlu, a.nume, c.an_aparitie, c.ISBN, c.rating from carte c join autor a on a.id=c.id_autor";
    if(msg[2]=='3')
        sql += " where titlu like '%";
    else if(msg[2]=='2')
        sql += " where a.nume like '%";
    else if(msg[2]=='1')
        sql += " join gencarte gc on c.id=gc.id_carte join gen g on g.id=gc.id_gen where g.nume='";
    else if(msg[2]=='5')
        sql += " where ISBN='";
    else if(msg[2]=='4')
        sql += " where an_aparitie='";
    else if(msg[2]=='6')
        sql += " where rating='";
    sql += string(msg+4);
    if(msg[2]=='3' || msg[2]=='2')
        sql += "%';";
    else
        sql += "';";
    return sql;
}

void verificareAccesat(sqlite3* db, int id_user, string id_carte){
    string sqlCommand = "select ac.id from accesat ac join user u on u.id=ac.id_user join carte c on c.id=ac.id_carte where ac.id_user=" + to_string(id_user) + " and ac.id_carte=" + id_carte + ";";
    vector<string> accesat;
    int rc;
    rc = sqlite3_exec(db,sqlCommand.c_str(),getAllData,&accesat,NULL);
    accesat.shrink_to_fit();
    int cap = accesat.capacity();
    if(cap==0){
        ///clientul nu a accesat cartea
        sqlCommand = "INSERT INTO accesat('id_user','id_carte') VALUES ('" + to_string(id_user) + "','" + id_carte + "');";
        cout<<sqlCommand<<endl;
        rc = sqlite3_exec(db,sqlCommand.c_str(),0,NULL,NULL);
    }
}

void verificareCautare(sqlite3* db, int id_user, char cautare[], char tip){
    if(cautare[4]>90)
        cautare[4]-=32;
    string sqlCommand = "select s.id from search s join user u on u.id=s.id_user where s.id_user=" + to_string(id_user) + " and s.search='" + string(cautare+4) + "' and s.id_tip=";
    sqlCommand += tip;
    sqlCommand += ";";
    vector<string> searched;
    int rc;
    rc = sqlite3_exec(db,sqlCommand.c_str(),getAllData,&searched,NULL);
    searched.shrink_to_fit();
    int cap = searched.capacity();
    if(cap==0){
        ///clientul nu a mai efectuat cautarea
        sqlCommand = "INSERT INTO search('id_tip','id_user','search') VALUES ('";
        sqlCommand += tip;
        sqlCommand += "', '" + to_string(id_user) + "','" + string(cautare+4) + "');";
        ///Inserarea in baza de date a cautarii
        rc = sqlite3_exec(db,sqlCommand.c_str(),0,NULL,NULL);
        cout<<sqlCommand<<endl;
    }
}

int ratingOption(sqlite3* db,char msgrasp[], int id_user, string id, int client){
    string sqlCommand;
    int rc;
    sqlCommand = "select r.rating from rate r join user u on u.id=r.id_user join carte c on c.id=r.id_carte where c.id=" + id + " and u.id=" + to_string(id_user) + ";";
    vector<string> rating;
    rc = sqlite3_exec(db,sqlCommand.c_str(),getAllData,&rating,NULL);
    rating.shrink_to_fit();
    int cap = rating.capacity();
    clearMsg(msgrasp);
    read(client,msgrasp,3);
    try{
        float x = stof(msgrasp);
        if(cap>0){
            ///clientul a dat deja rating cartii
            sqlCommand = "UPDATE rate SET rating=" + string(msgrasp) + " where id_carte=" + id + " and id_user=" + to_string(id_user) + ";";
        }
        else{
            ///clientul nu a dat rating cartii
            sqlCommand = "INSERT INTO rate('id_user','id_carte','rating') VALUES ('" + to_string(id_user) + "','" + id + "','" + string(msgrasp) + "');";
        }
        cout<<sqlCommand<<endl;
        rc = sqlite3_exec(db,sqlCommand.c_str(),0,NULL,NULL);
        ///reactualizez ratingul cartii
        ///trebuie sa iau toate rating-urile cartii cu id-ul id
        sqlCommand = "SELECT rating FROM rate where id_carte=" + id + ";";
        rating.clear();
        rc = sqlite3_exec(db,sqlCommand.c_str(),getAllData,&rating,NULL);
        float newRating = getNewRating(rating);
        ///trebuie sa o adaug in tabelul carte, sa alterez valoarea actuala
        sqlCommand = "UPDATE carte SET rating='" + to_string(newRating) + "' where id=" + id + ";";
        rc = sqlite3_exec(db,sqlCommand.c_str(),0,NULL,NULL);
        cout<<sqlCommand<<endl;
    }
    catch(const invalid_argument& ia){
        cout<<"Invalid!"<<endl;
        return 0;
    }
    return 1;
}

void logare(vector<string> usersList, int client, sqlite3* db, char msg[], char msgrasp[]){
    char msgrasp1[200];
    int rc;
    while(true){
        ///username
        clearMsg(msgrasp);
        clearMsg(msgrasp1);
        read (client, msgrasp, 100);
        read (client, msgrasp, 100);
        ///parola
        read (client, msgrasp1, 100);
        read (client, msgrasp1, 100);
        if(msg[0]=='1'){
            ///verific daca exista utilizator cu numele asta
            if(verifyUsername(msgrasp,usersList)==false){
                ///ii mai cer o data username
                write (client, "Nu exista un cont cu acest nume.", 200);
                clearMsg(msgrasp);
            }
            else if(verifyUser(msgrasp,msgrasp1,usersList)==false){
                ///il trimiti la primul login
                ///sau il scot din program
                write (client, "Parola incorecta.", 200);
                clearMsg(msgrasp);
            }
            else{
                write (client, "Conectare efectuata cu succes.", 200);
                break;
            }
        }
        else if(msg[0]=='2'){
            ///verific daca numele de utilizator este deja in baza de date
            if(verifyUsername(msgrasp,usersList)==true){
                ///ii mai cer o data username
                write (client, "Exista deja un cont cu acest nume.", 200);
                clearMsg(msgrasp);
            }
            else{
                write (client, "Contul a fost creat. Ati fost logat automat.", 200);
                string sqlCommand = "INSERT INTO User('username','parola') VALUES ('" + string(msgrasp) + "','" + string(msgrasp1) + "');";
                rc = sqlite3_exec(db,sqlCommand.c_str(),0,NULL,NULL);
                break;
            }
        }
    }
}

void verificareDownload(sqlite3* db, int id_user, string id_carte){
    string sqlCommand = "select d.id from descarcat d join user u on u.id=d.id_user join carte c on c.id=d.id_carte where d.id_user=" + to_string(id_user) + " and d.id_carte=" + id_carte + ";";
    vector<string> accesat;
    int rc;
    rc = sqlite3_exec(db,sqlCommand.c_str(),getAllData,&accesat,NULL);
    accesat.shrink_to_fit();
    int cap = accesat.capacity();
    if(cap==0){
        ///clientul nu a accesat cartea
        sqlCommand = "INSERT INTO descarcat('id_user','id_carte') VALUES ('" + to_string(id_user) + "','" + id_carte + "');";
        cout<<sqlCommand<<endl;
        rc = sqlite3_exec(db,sqlCommand.c_str(),0,NULL,NULL);
    }
}

void updatePreferinteGenuri(sqlite3* db,int id_user){
    string sqlCommand;
    int rc;
    vector<string> genuri, gen;

    sqlCommand = "delete from genuser where id_user=" + to_string(id_user) + ";";
    rc = sqlite3_exec(db,sqlCommand.c_str(),0,NULL,NULL);

    sqlCommand = "select count(*), g.nume from descarcat d join carte c on d.id_carte=c.id join gencarte gc on gc.id_carte=c.id join gen g on gc.id_gen=g.id where d.id_user=" + to_string(id_user) + " group by g.nume;";
    rc = sqlite3_exec(db,sqlCommand.c_str(),getAllData,&genuri,NULL);

    int totalGenuri=0;
    int sizeG = genuri.size();
    for(int i=0; i<sizeG; i+=2)
        totalGenuri += stoi(genuri[i]);
    for(int i=0; i<sizeG-2;i+=2)
        for(int j=i+2; j<sizeG; j+=2)
            if(genuri[i]<genuri[j]){
                swap(genuri[i],genuri[j]);
                swap(genuri[i+1],genuri[j+1]);
            }
    int i=0;
    while(true){
        float pct;
        pct = float(stoi(genuri[i]))/float(totalGenuri)*100;
        gen.clear();
        sqlCommand = "select id from gen where nume='" + genuri[i+1] + "';";
        rc = sqlite3_exec(db,sqlCommand.c_str(),getAllData,&gen,NULL);
        sqlCommand = "insert into genuser(id_user,id_gen) values (" + to_string(id_user) + "," + gen[0] + ");";
        rc = sqlite3_exec(db,sqlCommand.c_str(),0,NULL,NULL);
        cout<<sqlCommand<<endl;
        if(pct>=60){
            break;
        }
        if(pct>=40){
            pct = float(stoi(genuri[i+2]))/float(totalGenuri)*100;
            if(pct<20)
                break;
        }
        else if(pct>=30){
            pct = float(stoi(genuri[i+2]))/float(totalGenuri)*100;
            if(pct<16)
                break;
        }
        if(i>=4)
            break;
        i+=2;
    }
}

void recomandari(sqlite3* db, int id_user, Books *bookList){
    int rc;
    string sqlCommand;
    vector<string> cartiNecitite, genuriA, autoriA;
    vector<string> genuri, autori;
    unsigned totalGenuri=0, totalAutori=0;
    unsigned sizeAA, sizeGA, sizeG, sizeA;

    sqlCommand = "select count(*), g.nume from descarcat d join carte c on d.id_carte=c.id join gencarte gc on gc.id_carte=c.id join gen g on gc.id_gen=g.id where d.id_user=" + to_string(id_user) + " group by g.nume;";
    rc = sqlite3_exec(db,sqlCommand.c_str(),getAllData,&genuriA,NULL);
    ///3 Fantastic 4 Plitiste 2 Comedie
    sqlCommand = "select count(*), a.nume from descarcat d join carte c on d.id_carte=c.id join autor a on c.id_autor=a.id where d.id_user=" + to_string(id_user) + " group by a.nume;";
    rc = sqlite3_exec(db,sqlCommand.c_str(),getAllData,&autoriA,NULL);
    ///1 Dan Brown 3 J.K. Rowling

    sqlCommand = "select id from carte where id not in (select c.id from carte c join descarcat d on d.id_carte=c.id where d.id_user=" + to_string(id_user) + ");";
    rc = sqlite3_exec(db,sqlCommand.c_str(),getAllData,&cartiNecitite,NULL);
    ///id-urile cartilor necitite

    sqlCommand = "select nume from autor;";
    rc = sqlite3_exec(db,sqlCommand.c_str(),getAllData,&autori,NULL);
    ///toti autorii
    sqlCommand = "select nume from gen;";
    rc = sqlite3_exec(db,sqlCommand.c_str(),getAllData,&genuri,NULL);
    ///toate genurile

    sizeAA = autoriA.size();
    sizeGA = genuriA.size();
    sizeA = autori.size();
    sizeG = genuri.size();

    float pctGen[genuri.size()] = {0}, pctAutor[autori.size()] = {0};

    for(int i=0; i<sizeAA; i+=2)
        totalAutori += stoi(autoriA[i]);
    for(int i=0; i<sizeGA; i+=2)
        totalGenuri += stoi(genuriA[i]);

    ///crearea de punctaj pentru genurile citite
    for(int i=0; i<sizeGA; i+=2){
        float pct = 0;
        pct = (float(stoi(genuriA[i])) / float(totalGenuri))*100;
        if(pct>=80)
            pct=pct*0.3;
        else if(pct>=60 && pct < 80)
                pct=pct*0.4;
        else if(pct>=25 && pct < 55)
            pct=pct*0.6;
        else if(pct>10 && pct < 25)
            pct=pct*0.9;
        else
            pct=pct;
        pct*=0.1;
        for(int j=0; j<sizeG; j++){
            if(genuri[j]==genuriA[i+1])
                pctGen[j]=pct;
        }
    }
    genuriA.clear();

    ///crearea de punctaj pentru autorii cititi
    for(int i=0; i<sizeAA; i+=2){
        float pct = 0;
        pct = (float(stoi(autoriA[i])) / float(totalAutori))*100;
        if(pct>=80)
            pct=pct*0.17;
        else if(pct>=60 && pct < 80)
                pct=pct*0.26;
        else if(pct>=40 && pct < 60)
                pct=pct*0.4;
        else if(pct>=25 && pct < 40)
            pct=pct*0.45;
        else
            pct=pct*0.5;
        ///un autor nu ar trebui sa conteze la punctaj la fel de mult precum un gen preferat
        ///il reduc la 40% din punctajul lui actual
        pct=pct*0.4;
        pct*=0.1;
        for(int j=0; j<sizeA; j++){
            if(autori[j]==autoriA[i+1])
                pctAutor[j]=pct;
        }
    }
    autoriA.clear();

    sqlCommand = "select count(*), a.nume from accesat ac join carte c on ac.id_carte=c.id join autor a on c.id_autor=a.id where ac.id_user=" + to_string(id_user) + " group by a.nume;";
    rc = sqlite3_exec(db,sqlCommand.c_str(),getAllData,&autoriA,NULL);
    ///12 Mihail Drumes 3 J.K. Rowling
    sqlCommand = "select count(*), g.nume from accesat ac join carte c on ac.id_carte=c.id join gencarte gc on gc.id_carte=c.id join gen g on gc.id_gen=g.id where ac.id_user=" + to_string(id_user) + " group by g.nume;";
    rc = sqlite3_exec(db,sqlCommand.c_str(),getAllData,&genuriA,NULL);
    ///29 Fantastic 41 Teatru 11 Comedie
    sizeAA = autoriA.size();
    sizeGA = genuriA.size();

    totalAutori=totalGenuri=0;
    for(int i=0; i<sizeAA; i+=2)
        totalAutori += stoi(autoriA[i]);
    for(int i=0; i<sizeGA; i+=2)
        totalGenuri += stoi(genuriA[i]);

    ///crearea de punctaj pentru genurile accesate
    for(int i=0; i<sizeGA; i+=2){
        float pct = 0;
        pct = (float(stoi(genuriA[i])) / float(totalGenuri))*100;
        if(pct>=80)
            pct=pct*0.3;
        else if(pct>=60 && pct < 80)
                pct=pct*0.4;
        else if(pct>=25 && pct < 55)
            pct=pct*0.6;
        else if(pct>10 && pct < 25)
            pct=pct*0.9;
        else
            pct=pct;
        ///punctajele genurilor accesate nu este la fel de important precum al genurilor citite
        pct*=0.7;
        pct*=0.1;
        for(int j=0; j<sizeG; j++){
            if(genuri[j]==genuriA[i+1])
                pctGen[j]+=pct;
        }
    }
    genuriA.clear();

    ///crearea de punctaj pentru autorii accesati
    for(int i=0; i<sizeAA; i+=2){
        float pct = 0;
        pct = (float(stoi(autoriA[i])) / float(totalAutori))*100;
        if(pct>=80)
            pct=pct*0.17;
        else if(pct>=60 && pct < 80)
                pct=pct*0.26;
        else if(pct>=40 && pct < 60)
                pct=pct*0.4;
        else if(pct>=25 && pct < 40)
            pct=pct*0.45;
        else
            pct=pct*0.5;
        ///un autor nu ar trebui sa conteze la punctaj la fel de mult precum un gen preferat
        ///il reduc la 40% din punctajul lui actual
        pct=pct*0.4;
        pct*=0.6;
        pct*=0.1;
        for(int j=0; j<sizeA; j++){
            if(autori[j]==autoriA[i+1])
                pctAutor[j]+=pct;
        }
    }
    autoriA.clear();

    sqlCommand = "select count(*), s.search from search s join user u on u.id=s.id_user where s.id_user=" + to_string(id_user) + " and s.id_tip=1 group by s.search;";
    rc = sqlite3_exec(db,sqlCommand.c_str(),getAllData,&genuriA,NULL);
    ///5 Fantastic
    sqlCommand = "select count(*), s.search from search s join user u on u.id=s.id_user where s.id_user=" + to_string(id_user) + "  and s.id_tip=2 group by s.search;";
    rc = sqlite3_exec(db,sqlCommand.c_str(),getAllData,&autoriA,NULL);
    ///5 J.K. Rowling
    sizeAA = autoriA.size();
    sizeGA = genuriA.size();

    totalAutori=totalGenuri=0;
    for(int i=0; i<sizeAA; i+=2)
        totalAutori += stoi(autoriA[i]);
    for(int i=0; i<sizeGA; i+=2)
        totalGenuri += stoi(genuriA[i]);

    ///crearea de punctaj pentru genurile cautate
    for(int i=0; i<sizeGA; i+=2){
        float pct = 0;
        pct = (float(stoi(genuriA[i])) / float(totalGenuri))*100;
        if(pct>=80)
            pct=pct*0.3;
        else if(pct>=60 && pct < 80)
                pct=pct*0.4;
        else if(pct>=25 && pct < 55)
            pct=pct*0.6;
        else if(pct>10 && pct < 25)
            pct=pct*0.9;
        else
            pct=pct;
        ///punctajele genurilor accesate nu este la fel de important precum al genurilor citite
        pct*=0.4;
        pct*=0.1;
        for(int j=0; j<sizeG; j++){
            if(genuri[j]==genuriA[i+1])
                pctGen[j]+=pct;
        }
    }
    genuriA.clear();

    ///crearea de punctaj pentru autorii cautati
    for(int i=0; i<sizeAA; i+=2){
        float pct = 0;
        pct = (float(stoi(autoriA[i])) / float(totalAutori))*100;
        if(pct>=80)
            pct=pct*0.17;
        else if(pct>=60 && pct < 80)
                pct=pct*0.26;
        else if(pct>=40 && pct < 60)
                pct=pct*0.4;
        else if(pct>=25 && pct < 40)
            pct=pct*0.45;
        else
            pct=pct*0.5;
        ///un autor nu ar trebui sa conteze la punctaj la fel de mult precum un gen preferat
        ///il reduc la 40% din punctajul lui actual
        pct=pct*0.4;
        pct*=0.1;
        pct*=0.7;
        for(int j=0; j<sizeA; j++){
            if(autori[j]==autoriA[i+1])
                pctAutor[j]+=pct;
        }
    }
    autoriA.clear();

    int sizeCN = cartiNecitite.size();
    float pctCarti[sizeCN+1]={0};
    for(int i=0; i<sizeCN; i++){
        ///iau genurile cartii
        sqlCommand = "select g.nume from gencarte gc join gen g on g.id=gc.id_gen join carte c on c.id=gc.id_carte where  c.id=" + cartiNecitite[i] + ";";
        rc = sqlite3_exec(db,sqlCommand.c_str(),getAllData,&genuriA,NULL);;
        ///iau autorul cartii
        sqlCommand = "select a.nume from carte c join autor a on c.id_autor=a.id where c.id=" + cartiNecitite[i] + ";";
        rc = sqlite3_exec(db,sqlCommand.c_str(),getAllData,&autoriA,NULL);;

        ///ii dau punctele conform pctGen si pctAutor
        for(int j=0; j<sizeA; j++)
            if(autori[j]==autoriA[0])
                pctCarti[i] += pctAutor[j];

        ///actualizand vectorul pctCarti
        int gensize = genuriA.size();
        for(int j=0; j<sizeG; j++)
            for(int k=0; k<gensize; k++)
                if(genuri[j]==genuriA[k])
                    pctCarti[i] += pctGen[j];

        autoriA.clear();
        genuriA.clear();
    }

    genuriA.shrink_to_fit();
    autoriA.shrink_to_fit();
    ///fac un query ca sa gasesc userii cu aceleasi gusturi ca el (primele 3 genuri)
    ///in vectorul utilizatoriAsemanatori
    vector<string> gusturiUser, utilizatoriAsemanatori;
    sqlCommand = "select id_gen from genuser where id_user=" + to_string(id_user) + ";";
    rc = sqlite3_exec(db,sqlCommand.c_str(),getAllData,&gusturiUser,NULL);

    int sizeGU = gusturiUser.size();
    sqlCommand = "select distinct id_user from genuser where id_user!=" + to_string(id_user) + " and id_gen in (";
    for(int i=0; i<sizeGU; i++){
        sqlCommand += gusturiUser[i];
        if(i!=sizeGU-1)
            sqlCommand += ",";
    }
    sqlCommand += ");";
    rc = sqlite3_exec(db,sqlCommand.c_str(),getAllData,&utilizatoriAsemanatori,NULL);

    int sizeu = utilizatoriAsemanatori.size();
    //for(fiecare user asemanator)
    if(sizeu>0){
        sqlCommand = "select r.id_carte, r.rating from rate r join recomandat rr on r.id_carte=rr.id_carte where rr.id_user=r.id_user and r.id_user in (";
        for(int i=0; i<sizeu; i++){
            ///iau recomandarile userilor asemanatori
            sqlCommand += utilizatoriAsemanatori[i];
            if(i!=sizeu-1)
                sqlCommand += ",";
        }
        sqlCommand += ");";
        vector<string> recomandari;
        rc = sqlite3_exec(db,sqlCommand.c_str(),getAllData,&recomandari,NULL);
        int sizer = recomandari.size();
        //for(fiecare carte recomandata)
        for(int i=1; i<sizer; i+=2){
            ///iau ratingul cartii
            for(int j=0; j<sizeCN; j++){
                if(cartiNecitite[j]==recomandari[i-1]){
                    ///dau sau scad puncte in functie de rating
                    float rate = stof(recomandari[i]);
                    if(rate>4.5)
                        pctCarti[j] += 0.8;
                    else if(rate>3.5)
                        pctCarti[j] += 0.6;
                    else if(rate>3)
                        pctCarti[j] += 0.3;
                    else if(rate>2.5)
                        pctCarti[j] -= 0.2;
                    else if(rate>1.5)
                        pctCarti[j] -= 0.3;
                    else if(rate>0.5)
                        pctCarti[j] -= 0.45;
                    else
                        pctCarti[j] -= 0.55;
                    break;
                }
            }
        }
        recomandari.clear();

        sqlCommand = "select r.id_carte, r.rating from rate r where r.id_user in (";
        for(int i=0; i<sizeu; i++){
            ///iau recomandarile userilor asemanatori
            sqlCommand += utilizatoriAsemanatori[i];
            if(i!=sizeu-1)
                sqlCommand += ",";
        }
        sqlCommand += ");";
        rc = sqlite3_exec(db,sqlCommand.c_str(),getAllData,&recomandari,NULL);
        sizer = recomandari.size();
        for(int i=1; i<sizer; i+=2){
            ///iau ratingul cartii
            for(int j=0; j<sizeCN; j++){
                if(cartiNecitite[j]==recomandari[i-1]){
                    ///dau sau scad puncte in functie de rating
                    float rate = stof(recomandari[i]);
                    if(rate>4.5)
                        pctCarti[j] += 0.4;
                    else if(rate>4)
                        pctCarti[j] += 0.3;
                    else if(rate>3.5)
                        pctCarti[j] += 0.2;
                    break;
                }
            }
        }
    }

    ///sortez vectorul pctCarti, modificand si cartiNecitite
    for(int i=0; i<sizeCN-1; i++)
        for(int j=i+1; j<sizeCN; j++)
            if(pctCarti[i]<pctCarti[j]){
                swap(pctCarti[i],pctCarti[j]);
                swap(cartiNecitite[i],cartiNecitite[j]);
            }

    if(pctCarti[0]==0){
        ///inseamna ca nu are nimic in istoric si nimic in comun cu nimeni
        sqlCommand = "select c.id, titlu, a.nume, an_aparitie from carte c join autor a on c.id_autor=a.id  order by rating desc limit 15;";
        rc = sqlite3_exec(db,sqlCommand.c_str(),getAllData,bookList,NULL);
    }
    else{
        ///extrag primele 15 carti din cartiNecitite si le trimit inapoi prin vectorul bookList
        sqlCommand = "select c.id, titlu, a.nume, an_aparitie from carte c join autor a on c.id_autor=a.id where c.id in (";
        for(int i=0; i<=14;i++){
            sqlCommand += cartiNecitite[i];
            if(i<14)
                sqlCommand += ",";
        }
        sqlCommand += ");";
        rc = sqlite3_exec(db,sqlCommand.c_str(),getAllData,bookList,NULL);
    }

    ///inserez in baza de date recomandarile pentru userul cu id_user
    int j=0;
    for(int i=1; i<=15; i++){
        sqlCommand = "insert into recomandat(id_user,id_carte) values(" + to_string(id_user) + "," + cartiNecitite[j] + ");";
        rc = sqlite3_exec(db,sqlCommand.c_str(),0,NULL,NULL);
        j+=4;
    }
}
