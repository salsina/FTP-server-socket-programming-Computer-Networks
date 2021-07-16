#ifdef WINDOWS
#include <direct.h>
#define GetCurrentDir _getcwd
#else
#include <unistd.h>
#define GetCurrentDir getcwd
#endif

#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <string.h>
#include <stdio.h>
#include <vector>
#include "json-develop/single_include/nlohmann/json.hpp"
#include <bits/stdc++.h> 
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h> 
#include <dirent.h>
#include <ftw.h>


using json = nlohmann::json;
using namespace std;
char sentence[4085];
char buffer[2047];
float states[100];
vector<int> sockfds;
vector<string> sockfd_usernames;
vector<string> file_names;

class User{
    public:
    User(string _username,string _password, string _admin, string _size){
        username = _username;
        password = _password;
        admin = _admin == "true" ? true : false ;
        size = stoi(_size);
        char buff[2047]; //create string buffer to hold path
        GetCurrentDir( buff, 2047 );
        string current_working_dir(buff);
        curr_location = current_working_dir;
        first_location = curr_location;
    }
    string get_username(){ return username; }
    bool passwords_match(string pass){ return pass == password; }
    void log_in(){ login = true; }
    void log_out(){ login = false; }
    bool is_logged_in(){ return login; }
    bool is_admin(){ return admin; }
    bool can_dl(int filesize) { return filesize <= size; }
    void DL(int filesize){ size -= filesize; }
    string get_path(){ return curr_location; }
    string get_first_path(){ return first_location; }
    void change_curr_dir(string new_dir){ curr_location = new_dir; }
    void set_got_username(bool status){ got_username = status; }
    bool has_got_username(){ return got_username; }
    private:
        string first_location;
        string curr_location;
        string username ;
        string password ;
        bool admin ;
        int size ;
        bool got_username;
        bool login;
};
vector<User*> Users;

void write_text_to_log_file( const string &text )
{
    ofstream log_file(
        "log_file.txt", ios_base::out | ios_base::app );
    log_file << text << endl;
}

string curr_time(){
    auto timenow =
      chrono::system_clock::to_time_t(chrono::system_clock::now());
    return ctime(&timenow) ;
}

static int rmFiles(const char *pathname, const struct stat *sbuf, int type, struct FTW *ftwb)
{
    if(remove(pathname) < 0)
    {
        cout<<("ERROR: remove");
        return -1;
    }
    return 0;
}

void write_to_client(int sockfd,string message)
{
    const char *message_char = message.c_str();

    if(write(sockfd,message_char,
        strlen(message_char)) < 0)
    {
        write(2,"500: Error\n",10);
        exit(1);
    }
}

string find_sockfd_username(int sockfd)
{
    for(int i = 0;i<sockfds.size();i++)
    {
        if(sockfds[i] == sockfd )
        {
            return sockfd_usernames[i];
        }
    }
    return "";
}

User* find_user_by_sockfd(int sockfd)
{
    string username = find_sockfd_username(sockfd);
    for(int i=0;i<Users.size();i++)
    {
        if (Users[i]->get_username() == username)
            return Users[i];
    }
    return NULL;
}

bool is_user_logged_in(int sockfd)
{
    string username = find_sockfd_username(sockfd);
    for(int i=0;i<Users.size();i++)
    {
        if (Users[i]->get_username() == username)
            return Users[i]->is_logged_in();
    }
    return false;
}

bool is_user_admin(int sockfd)
{
    string username = find_sockfd_username(sockfd);
    for(int i=0;i<Users.size();i++)
    {
        if (Users[i]->get_username() == username)
            return Users[i]->is_admin();
    }
    return false;
}

bool can_access_file(int sockfd,string filename)
{
    if(is_user_admin(sockfd))
        return true;
    
    for(int i=0;i<file_names.size();i++)
    {
        if(filename == file_names[i])
            return false;
    }
    return true;
}

void read_and_create_users(json j){

    for(int i=0;i<j["users"].size();i++){
        json user = j["users"][i];
        string username = user["user"];
        string password = user["password"];
        string admin = user["admin"];
        string size = user["size"];
        User* new_user = new User(username, password, admin, size);
        Users.push_back(new_user);
    }
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void handle_sockfd_usernames(int sockfd,string username)
{
    for(int i = 0;i<sockfds.size();i++)
    {
        if(sockfds[i] == sockfd){
            sockfd_usernames[i] = username;
            return;
        }
    }
    sockfds.push_back(sockfd);
    sockfd_usernames.push_back(username);
}

void login_username(int sockfd,string username)
{
    for(int i=0;i<Users.size();i++)
    {
        if (Users[i]->get_username() == username){
            handle_sockfd_usernames(sockfd,username);
            write_to_client(sockfd,"331:‬‬ ‫‪User‬‬ ‫‪name‬‬ ‫‪okay,‬‬ ‫‪need‬‬ ‫‪password.");
            Users[i]->set_got_username(true);
            return;
        }
    }
    write_to_client(sockfd,"‫‪‫‪430:‬‬ ‫‪Invalid‬‬ ‫‪username‬‬ ‫‪or‬‬ ‫‪password‬‬‬‬");
}


void login_password(int sockfd,string password)
{
    string sockfd_username = find_sockfd_username(sockfd);
    if (sockfd_username == "")
    {
        write_to_client(sockfd,"‫‪‫‪503:‬‬ ‫‪Bad‬‬ ‫‪sequence‬‬ ‫‪of‬‬ ‫‪commands.‬‬‬‬");
        return;
    }

    for(int i=0;i<Users.size();i++)
    {
        if(Users[i]->get_username() == sockfd_username){
            if(!Users[i]->has_got_username()){
                write_to_client(sockfd,"‫‪‫‪503:‬‬ ‫‪Bad‬‬ ‫‪sequence‬‬ ‫‪of‬‬ ‫‪commands.‬‬‬‬");
                return;
            }
            if (Users[i]->passwords_match(password) ){
                Users[i]->log_in();
                Users[i] ->set_got_username(false);
                write_text_to_log_file("User " + sockfd_username + " logged in at "+curr_time());
                write_to_client(sockfd,"‫‪230:‬‬ ‫‪User‬‬ ‫‪logged‬‬ ‫‪in,‬‬ ‫‪proceed.‬‬ ‫‪Logged‬‬ ‫‪out‬‬ ‫‪if‬‬ ‫‪appropriate.‬‬");
            }
            else
                write_to_client(sockfd,"‫‪‫‪430:‬‬ ‫‪Invalid‬‬ ‫‪username‬‬ ‫‪or‬‬ ‫‪password‬‬‬‬");
            return;
        }
    }
}

void pwd(int sockfd)
{
    if(!is_user_logged_in(sockfd)){
        write_to_client(sockfd,"‫‪332:‬‬ ‫‪Need‬‬ ‫‪account‬‬ ‫‪for‬‬ ‫‪login.‬‬");
        return;
    }

    User* curr_user = find_user_by_sockfd(sockfd);
    string user_path = curr_user->get_path();
    write_text_to_log_file("User " + curr_user->get_username() + " used pwd at "+curr_time());
    write_to_client(sockfd,"257: " + user_path);
}

void mkd(int sockfd,string path)
{
    if(!is_user_logged_in(sockfd)){
        write_to_client(sockfd,"‫‪332:‬‬ ‫‪Need‬‬ ‫‪account‬‬ ‫‪for‬‬ ‫‪login.‬‬");
        return;
    }

    char buff[2047]; 
    GetCurrentDir( buff, 2047 );
    string server_adr_string(buff);
    const char *server_adr_char = server_adr_string.c_str();

    User* curr_user = find_user_by_sockfd(sockfd);
    string adr_string = curr_user->get_path();
    const char *adr_char = adr_string.c_str();
    chdir(adr_char);


    const char *path_char = path.c_str();

	if (mkdir(path_char, 0777) == -1){
        write_to_client(sockfd,"‫‪500:‬‬ ‫‪Error‬‬");
    }
	else{
        write_to_client(sockfd,"257: " + path+" ‫‪created.‬‬");
    }
    chdir(server_adr_char);
    write_text_to_log_file("User " + curr_user->get_username() + " made directory "+path+" at "+curr_time());

}

void delete_file(int sockfd,string filename)
{
    if(!is_user_logged_in(sockfd)){
        write_to_client(sockfd,"‫‪332:‬‬ ‫‪Need‬‬ ‫‪account‬‬ ‫‪for‬‬ ‫‪login.‬‬");
        return;
    }

    if(!can_access_file(sockfd,filename)){
        write_to_client(sockfd,"‫‪‫‪550:‬‬ ‫‪File‬‬ ‫‪unavailable.‬‬‬‬");
        return;
    }

    char buff[2047]; 
    GetCurrentDir( buff, 2047 );
    string server_adr_string(buff);
    const char *server_adr_char = server_adr_string.c_str();

    User* curr_user = find_user_by_sockfd(sockfd);
    string adr_string = curr_user->get_path();
    const char *adr_char = adr_string.c_str();
    chdir(adr_char);

    const char *filename_char = filename.c_str();

    if(remove(filename_char) != 0)
    {
        write_to_client(sockfd,"‫‪500:‬‬ ‫‪Error‬‬");
        chdir(server_adr_char);
        return;
    }
    write_to_client(sockfd,"250: " + filename+" deleted.");
    chdir(server_adr_char);
    write_text_to_log_file("User " + curr_user->get_username() + " deleted file "+filename+" at "+curr_time());

}

void delete_dir(int sockfd, string dir_path)
{
    if(!is_user_logged_in(sockfd)){
        write_to_client(sockfd,"‫‪332:‬‬ ‫‪Need‬‬ ‫‪account‬‬ ‫‪for‬‬ ‫‪login.‬‬");
        return;
    }

    char buff[2047]; 
    GetCurrentDir( buff, 2047 );
    string server_adr_string(buff);
    const char *server_adr_char = server_adr_string.c_str();

    User* curr_user = find_user_by_sockfd(sockfd);
    string adr_string = curr_user->get_path();
    const char *adr_char = adr_string.c_str();
    chdir(adr_char);

    const char *dir_path_char = dir_path.c_str();

    if (nftw(dir_path_char, rmFiles,10, FTW_DEPTH|FTW_MOUNT|FTW_PHYS) < 0){
        write_to_client(sockfd,"‫‪500:‬‬ ‫‪Error‬‬");
        chdir(server_adr_char);
        return;
    }
	else{
        write_to_client(sockfd,"250: " + dir_path+" deleted.‬‬");
    }
    chdir(server_adr_char);
    write_text_to_log_file("User " + curr_user->get_username() + " deleted directory "+dir_path+" at "+curr_time());
}

void ls(int sockfd, int sockfd_data)
{
    if(!is_user_logged_in(sockfd)){
        write_to_client(sockfd_data,"‫‪");
        write_to_client(sockfd,"‫‪332:‬‬ ‫‪Need‬‬ ‫‪account‬‬ ‫‪for‬‬ ‫‪login.‬‬");
        return;
    }

    User* curr_user = find_user_by_sockfd(sockfd);
    string adr_string  = curr_user->get_path();
    const char *adr = adr_string.c_str();

    struct dirent *de; 
    DIR *dr = opendir(adr); 
    if (dr == NULL)   
    { 
        write_to_client(sockfd_data,"");
        write_to_client(sockfd,"‫‪500:‬‬ ‫‪Error‬‬");
        return ; 
    } 

    string dirs_to_pass = "";
    while ((de = readdir(dr)) != NULL)
    {
        char* name = de->d_name;
        if(name[0] != '.' )
        {
            string name_str(name);
            dirs_to_pass += name_str + '\n';
        }
    }
    closedir(dr);
    write_text_to_log_file("User " + curr_user->get_username() + " used ls at "+curr_time());
    write_to_client(sockfd_data,dirs_to_pass);
    write_to_client(sockfd,"‫‪226:‬‬ ‫‪List‬‬ ‫‪transfer‬‬ ‫‪done.‬‬");
}

bool DoesPathExist(const string &s)
{
  struct stat buffer;
  return (stat (s.c_str(), &buffer) == 0);
}

void cwd_NoArgs(int sockfd)
{
    if(!is_user_logged_in(sockfd)){
        write_to_client(sockfd,"‫‪332:‬‬ ‫‪Need‬‬ ‫‪account‬‬ ‫‪for‬‬ ‫‪login.‬‬");
        return;
    }

    User* curr_user = find_user_by_sockfd(sockfd);
    string start_dir = curr_user->get_first_path();
    curr_user->change_curr_dir(start_dir);
    write_text_to_log_file("User " + curr_user->get_username() + " changed directory to "+start_dir+" at "+ curr_time());
    write_to_client(sockfd,"‫‪‫‪250:‬‬ ‫‪Successful‬‬ ‫‪change.‬‬‬‬");
}

void cwd(int sockfd,string path)
{
    if(!is_user_logged_in(sockfd)){
        write_to_client(sockfd,"‫‪332:‬‬ ‫‪Need‬‬ ‫‪account‬‬ ‫‪for‬‬ ‫‪login.‬‬");
        return;
    }

    char buff[2047]; 
    GetCurrentDir( buff, 2047 );
    string server_adr_string(buff);
    const char *server_adr_char = server_adr_string.c_str();

    User* curr_user = find_user_by_sockfd(sockfd);
    string user_path = curr_user -> get_path();
    const char *user_path_char = user_path.c_str();
    
    if(chdir(user_path_char) == 0)
    {
        const char *path_char = path.c_str();
        if(chdir(path_char) != 0){
            write_to_client(sockfd,"‫‪500:‬‬ ‫‪Error‬‬");
            chdir(server_adr_char);
            return;
        }
        char buff[2047];
        GetCurrentDir( buff, 2047 );
        string new_client_path(buff);

        curr_user->change_curr_dir(new_client_path);
        write_to_client(sockfd,"‫‪‫‪250:‬‬ ‫‪Successful‬‬ ‫‪change.‬‬‬‬");
        chdir(server_adr_char);
        write_text_to_log_file("User " + curr_user->get_username() + " changed directory to "+curr_user->get_path()+" at "+ curr_time());
        return;
    }

}

void rename(int sockfd, string from,string to)
{
    if(!is_user_logged_in(sockfd)){
        write_to_client(sockfd,"‫‪332:‬‬ ‫‪Need‬‬ ‫‪account‬‬ ‫‪for‬‬ ‫‪login.‬‬");
        return;
    }

    if(!can_access_file(sockfd,from)){
        write_to_client(sockfd,"‫‪‫‪550:‬‬ ‫‪File‬‬ ‫‪unavailable.‬‬‬‬");
        return;
    }

    char buff[2047]; 
    GetCurrentDir( buff, 2047 );
    string server_adr_string(buff);
    const char *server_adr_char = server_adr_string.c_str();

    User* curr_user = find_user_by_sockfd(sockfd);
    string adr_string = curr_user->get_path();
    const char *adr_char = adr_string.c_str();
    chdir(adr_char);

    const char *from_char = from.c_str();
    const char *to_char = to.c_str();
    if(rename(from_char,to_char) != 0)
    {
        write_to_client(sockfd,"‫‪500:‬‬ ‫‪Error‬‬");
        return;
    }
    write_to_client(sockfd,"‫‪250:‬‬ ‫‪Successful‬‬ ‫‪change.‬‬");
    chdir(server_adr_char);
    write_text_to_log_file("User " + curr_user->get_username() + " changed filename "+from+" to "+to+" at "+ curr_time());
}

void send_downloaded_file(int sockfd_data, string filename)
{
    const char *filename_char = filename.c_str();
    string result = "";
    string temp;
    ifstream file(filename_char);
    while(getline(file,temp))
    {
        result += temp + '\n';
    }
    write_to_client(sockfd_data,result);
}

int download_if_possible(int sockfd,int sockfd_data,int file_size, string filename)
{
    string username = find_sockfd_username(sockfd);
    for(int i=0;i<Users.size();i++)
    {
        if(Users[i]->get_username() == username){
            if(Users[i]->can_dl(file_size)){
                Users[i]->DL(file_size);
                send_downloaded_file(sockfd_data,filename);
                // write_text_to_log_file("User " + username + " downloaded file "+filename+" at "+ curr_time());
                write_to_client(sockfd,"‫‪‫‪226:‬‬ ‫‪Successful‬‬ ‫‪Download.‬‬‬‬");
                return 1;
            }
            else{
                write_to_client(sockfd,"‫‪‫‪‫‪425:‬‬ ‫‪Can't‬‬ ‫‪open‬‬ ‫‪data‬‬ ‫‪connection.‬‬");
                return 0;
            }
        }
    }
    return 0;
}

inline bool DoesFileExist(const string &name){
  ifstream f(name.c_str());
  return f.good();
}

void retr(int sockfd,int sockfd_data, string filename)
{
    if(!is_user_logged_in(sockfd)){
        write_to_client(sockfd_data,"‫‪");
        write_to_client(sockfd,"‫‪332:‬‬ ‫‪Need‬‬ ‫‪account‬‬ ‫‪for‬‬ ‫‪login.‬‬");
        return;
    }

    if(!can_access_file(sockfd,filename)){
        write_to_client(sockfd_data,"‫‪");
        write_to_client(sockfd,"‫‪‫‪550:‬‬ ‫‪File‬‬ ‫‪unavailable.‬‬‬‬");
        return;
    }

    char buff[2047]; 
    GetCurrentDir( buff, 2047 );
    string server_adr_string(buff);
    const char *server_adr_char = server_adr_string.c_str();

    User* curr_user = find_user_by_sockfd(sockfd);
    string adr_string = curr_user->get_path();
    const char *adr_char = adr_string.c_str();
    chdir(adr_char);


    if(DoesFileExist(filename) == 0){
        write_to_client(sockfd_data,"");
        write_to_client(sockfd,"‫‪500:‬‬ ‫‪Error‬‬");
        chdir(server_adr_char);
        return;
    }
    ifstream in_file(filename, ios::binary);
    in_file.seekg(0, ios::end);
    int file_size = in_file.tellg();

    int ans = download_if_possible(sockfd,sockfd_data,file_size, filename);
    chdir(server_adr_char);
    if (ans == 1)
        write_text_to_log_file("User " + curr_user->get_username() + " downloaded file "+filename+" at "+ curr_time());
}

void show_commands(int sockfd)
{
    string help = "‫‪214‬‬\n‫‪user‬‬ ‫‪[name],‬‬ ‫‪Its‬‬ ‫‪argument‬‬ ‫‪is‬‬ ‫‪used‬‬ ‫‪to‬‬ ‫‪specify‬‬ ‫‪the‬‬ ‫‪user’s‬‬ ‫‪string.‬‬ ‫‪It‬‬ ‫‪is‬‬ ‫‪used‬‬ ‫‪for‬‬ ‫‪user‬‬ ‫‪authentication.‬‬\n\
pass ‫‪[password],‬‬ ‫‪Its‬‬ ‫‪argument‬‬ ‫‪is‬‬ ‫‪used‬‬ ‫‪to‬‬ ‫‪specify‬‬ ‫‪the‬‬ ‫‪password’s‬‬ ‫‪string.‬‬ ‫‪It‬‬ ‫‪is‬‬ ‫‪used‬‬ ‫‪for‬‬ ‫‪user‬‬ ‫‪authentication.‬‬\n\
pwd‬‬ , It takes no arguments. It is used for showing the current directory path.\n\
‫‪mkd‬‬ ‫‪[directory‬‬ ‫‪path]‬‬, Its argument is used to specify the path of the new directory. It is used for creating new directory.\n\
dele -f [filename], Its argument is used to specify the name of file. It is used for deleting a file.\n\
dele -d [directory path], Its argument is used to specify the path of directory. It is used for deleting a directory.\n\
ls , It takes no arguments. It is used for showing files inside current directory.\n\
cwd [path], Its argument is used to specify the path of directory. It is used for changing the directory you are in.\n\
rename [from] [to], It is used to change the file name [from] to its new name [to].\n\
retr [name], Its argument is the file name. It is used to download the file.\n\
help , It takes no arguments. It is used for showing the commands you can enter.\n\
quit , It takes no arguments. It is used for logging out from account.\n";
    string username = find_sockfd_username(sockfd);
    write_text_to_log_file("User " + username + " used help at "+ curr_time());
    write_to_client(sockfd,help);

}

void logout(int sockfd)
{
    for(int i=0;i<Users.size();i++)
    {
        string sockfd_username = find_sockfd_username(sockfd);
        if (sockfd_username == "")
        {
            write_to_client(sockfd,"‫‪500:‬‬ ‫‪Error‬‬");
            return;
        }

        if(Users[i]->get_username() == sockfd_username){
            Users[i]->log_out();
            write_text_to_log_file("User " + sockfd_username + " logged out at "+ curr_time());
            write_to_client(sockfd,"‫‪‫‪221:‬‬ ‫‪Successful‬‬ ‫‪Quit.‬‬");
            return;
        }
    }
    write_to_client(sockfd,"‫‪500:‬‬ ‫‪Error‬‬");
    return;
}

vector<string> removeDupWord(string str) 
{ 
    vector<string> ans;
    string word = ""; 
    for (auto x : str)  
    { 
        if (x == ' ') 
        { 
            ans.push_back(word);
            word = ""; 
        } 
        else { 
            if(x!= '\n')
                word = word + x; 
        } 
    } 

    ans.push_back(word);
    return ans;
} 

bool syntax_is_valid(int sockfd,int command_size,int correct_size)
{
    if(command_size != correct_size){
        write_to_client(sockfd,"‫‪501:‬‬ ‫‪Syntax‬‬ ‫‪error‬‬ ‫‪in‬‬ ‫‪parameters‬‬ ‫‪or‬‬ ‫‪arguments.‬‬‬‬");
        return false;
    }
    return true;
}

void handle_commands(int sockfd, int sockfd_data, char* input)
{
    vector<string> splitted_input = removeDupWord(input);
    string command = splitted_input[0];

    if(command == "user"){
        if(syntax_is_valid(sockfd,splitted_input.size(),2))
            login_username(sockfd,splitted_input[1]);
    }
    else if(command == "pass"){
        if(syntax_is_valid(sockfd,splitted_input.size(),2))
            login_password(sockfd,splitted_input[1]);
    }
    else if(command == "pwd"){
        if(syntax_is_valid(sockfd,splitted_input.size(),1))
            pwd(sockfd);
    }
    else if(command == "mkd") {
        if(syntax_is_valid(sockfd,splitted_input.size(),2))
            mkd(sockfd,splitted_input[1]);
    }
    else if(command == "dele") 
    {
        if(syntax_is_valid(sockfd,splitted_input.size(),3))
        {
            if(splitted_input[1] == "-f")
                delete_file(sockfd,splitted_input[2]);
            else if(splitted_input[1] == "-d")
                delete_dir(sockfd,splitted_input[2]);
        }
    }
    else if(command == "ls"){
        if(syntax_is_valid(sockfd,splitted_input.size(),1))
            ls(sockfd,sockfd_data);
    }
    else if(command == "cwd") 
    {
        if (splitted_input.size() == 1)
            cwd_NoArgs(sockfd);
        else if(syntax_is_valid(sockfd,splitted_input.size(),2))
            cwd(sockfd,splitted_input[1]);
    }
    else if(command == "rename") {
        if(syntax_is_valid(sockfd,splitted_input.size(),3))
            rename(sockfd,splitted_input[1],splitted_input[2]);
    }
    else if(command == "retr") {
        if(syntax_is_valid(sockfd,splitted_input.size(),2))
            retr(sockfd,sockfd_data,splitted_input[1]);
    }
    else if(command == "help") {
        if(syntax_is_valid(sockfd,splitted_input.size(),1))
            show_commands(sockfd);
    }
    else if(command == "quit") {
        if(syntax_is_valid(sockfd,splitted_input.size(),1))
            logout(sockfd);
    }
    else 
        write_to_client(sockfd,"‫‪501:‬‬ ‫‪Syntax‬‬ ‫‪error‬‬ ‫‪in‬‬ ‫‪parameters‬‬ ‫‪or‬‬ ‫‪arguments.‬‬‬‬");
}

void create_file_names(json j)
{
    for(int i=0;i<j["files"].size();i++)
        file_names.push_back(j["files"][i]);

}
int main(){
    ifstream i("config.json");
    json j;
    i >> j;
    int commandChannelPort = j["commandChannelPort"];
    int dataChannelPort = j["dataChannelPort"];
    create_file_names(j);   
    read_and_create_users(j);

    int maximum_fd,serverfd,newfd,n, rv;

    struct sockaddr_storage remoteaddr; // client address

    socklen_t addrlen;

    char buf[256]; 

    char remoteIP[INET6_ADDRSTRLEN];

    int flag = 1;        

    struct addrinfo hints, *ai, *p;

    fd_set mainfds;
    fd_set read_fds;
    FD_ZERO(&mainfds); 
    FD_ZERO(&read_fds);

    memset(&hints, 0, sizeof hints);

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    char num_char[10 + sizeof(char)];

    std::sprintf(num_char, "%d", commandChannelPort);    

    if ((rv = getaddrinfo(NULL, num_char, &hints, &ai)) != 0) {
        write(2, "select error\n", 13);
        exit(1);
    }
    
    for(p = ai; p != NULL; p = p->ai_next) 
    {
        serverfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);

        if (serverfd < 0) 
            continue;
        
        setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int));

        if (bind(serverfd, p->ai_addr, p->ai_addrlen) < 0) 
        {
            close(serverfd);
            continue;
        }

        break;
    }

    if (p == NULL) 
    {
        write(1, "binding failed\n",15);
        exit(2);
    }

    freeaddrinfo(ai); 

    if (listen(serverfd, 10) == -1) 
    {
        write(2,"listen",6);
        exit(3);
    }

    FD_SET(serverfd, &mainfds);

    maximum_fd = serverfd; 

// creating data sockfd

    int serverfd_data , newfd_data  ;

    struct sockaddr_in serv_addr , cli_addr;
    socklen_t clilen;

    serverfd_data = socket(AF_INET , SOCK_STREAM , 0);
    if (serverfd_data < 0)
        write(2,"error opening socket",6);

    bzero((char*) &serv_addr , sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(dataChannelPort);

    if(bind(serverfd_data , (struct sockaddr *) &serv_addr , sizeof(serv_addr)) < 0 ) 
    {
        write(2,"error binding",6);
    }

    listen(serverfd_data , 5);
    clilen = sizeof(cli_addr);

    cout<<"server is running...\n";
// connection goes here

    for(;;)
    {
        read_fds = mainfds; 
        if (select(maximum_fd+1, &read_fds, NULL, NULL, NULL) == -1) 
        {
            write(2,"select",6);
            exit(4);
        }

        for(int i = 0; i <= maximum_fd; i++) 
        {
            if (FD_ISSET(i, &read_fds)) 
            { 
                if (i == serverfd) {

                    addrlen = sizeof (remoteaddr);

                    newfd = accept(serverfd,(struct sockaddr *)&remoteaddr,&addrlen);
                    newfd_data = accept(serverfd_data,(struct sockaddr *)&remoteaddr,&addrlen);

                    if (newfd == -1) 
                    {
                        write(2,"accept",6);
                    } 
                    else 
                    {
                        FD_SET(newfd, &mainfds); 
                        if (newfd > maximum_fd) 
                            maximum_fd = newfd;

                        sprintf(sentence,"selectserver: new connection from %s on socket %d\n",
                            inet_ntop(remoteaddr.ss_family,get_in_addr((struct sockaddr*)&remoteaddr),remoteIP, INET6_ADDRSTRLEN),
                            newfd);
                        puts(sentence);

                        string newfd_data_str = to_string(newfd_data);
                        const char *newfd_data_char = newfd_data_str.c_str();
                        if(write(newfd,newfd_data_char,sizeof(char*)) < 0)
                            write(2,"Error writing\n",14);
                    }

                }
                else{
                    
                    int command,bytes_received;
                    bzero(buffer,2047);
    

                    if ((bytes_received = recv(i, buffer, 2047, 0)) <= 0) 
                    {
                        if (bytes_received == 0) 
                        {
                            sprintf(sentence,"selectserver: socket %d hung up", i);
                            puts(sentence);
                        }
                        else 
                            write(2,"\n",0);
                        close(i); 
                        FD_CLR(i, &mainfds); 
                    } 
                    else{
                        sprintf(sentence,"cilent %d - command is : %s",i,buffer);
                        puts(sentence);
                        char curr_socket_data_port[2047];
                        if(read(i , curr_socket_data_port , 2047) < 0)
                            write(2,"Error reading\n",14);
                        int i_data = atoi(curr_socket_data_port);
                        handle_commands(i,i_data,buffer);
                    }
                }

            }

        }

    }
    return 0;
}