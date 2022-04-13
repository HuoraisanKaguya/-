#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <errno.h>
#include <iomanip>
#include <sys/sysinfo.h>
#include <signal.h>

using namespace std;

class ProcessStatistics {
public:
	int ppid;		//1
	int priority;	//3
	char state;		//4
	int virtMemory;	//5
	int resMemory;	//6	
	int threads;	//7
	
	int starttime;
	int utime;
	int stime;
	
	void read(const char *pid) {
		char path[32];
		snprintf(path, sizeof(path), "/proc/%s/stat", pid);
		FILE* fd = fopen(path, "r");
		
		fscanf(fd, " %*d");  	
		fscanf(fd, " %*s");
		fscanf(fd, " %c", &state);
		fscanf(fd, " %d", &ppid); 	
		fscanf(fd, " %*d");
		fscanf(fd, " %*d");
		fscanf(fd, " %*d");
		fscanf(fd, " %*d");
		fscanf(fd, " %*u");
		fscanf(fd, " %*u");
		fscanf(fd, " %*u");
		fscanf(fd, " %*u");
		fscanf(fd, " %*u");
		fscanf(fd, " %lu", &utime); 	
		fscanf(fd, " %lu", &stime); 
		fscanf(fd, " %*d");
		fscanf(fd, " %*d");
		fscanf(fd, " %d", &priority);
		fscanf(fd, " %*d");
		fscanf(fd, " %d", &threads); 
		fscanf(fd, " %*d");
		fscanf(fd, " %lu", &starttime);
		fscanf(fd, " %lu", &virtMemory); 
		fscanf(fd, " %lu", &resMemory);

		fclose(fd);
		return;
	}	
};

class Process {
	ProcessStatistics procStat;
	int pid;		//0
	string user;	//2	
	int cpu;		//8
	int clockTicks;	//9
	string time;	//10	
	string cmd;		//11
	
public:
	Process(const char* pidString, int oldClockTicks) {
		procStat.read(pidString);		
		
		pid = atoi(pidString);
		setUser();		
		setMemory();
		setClockTicks();
		setCpu(oldClockTicks);
		setTime();
		setCmd();
	}
	
	void setMemory() {
		procStat.virtMemory = procStat.virtMemory / 1024;
		procStat.resMemory = procStat.resMemory * sysconf(_SC_PAGE_SIZE) / 1024;
	}
	void setClockTicks() {
		clockTicks = procStat.utime + procStat.stime;
	}
	void setCpu(int oldClockTicks) {
		cpu = clockTicks - oldClockTicks;
	}
	void setTime() {
		struct sysinfo si;
		sysinfo(&si);
		int sysTime = si.uptime - (procStat.starttime/sysconf(_SC_CLK_TCK));
		stringstream ss;
		ss << setfill('0') << setw(2) << sysTime / 3600 
		<< ':' << setfill('0') << setw(2)<< (sysTime % 3600) / 60  
		<< ':' << setfill('0') << setw(2)<< (sysTime % 3600) % 60;
		time = ss.str();
	}
	void setUser() {
		struct stat sstat;
		struct passwd usrpwd, *res;
		char buf[1024];		
		char path[20];
		snprintf(path, sizeof(path), "/proc/%d", pid);
		stat(path, &sstat);
		getpwuid_r(sstat.st_uid, &usrpwd, buf, sizeof(buf), &res);
		user = usrpwd.pw_name;
	}
	void setCmd() {
		char path[32];
		snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
		int fd = open(path, O_RDONLY);
		const int MAX_CMD = 4096;
		char command[MAX_CMD + 1];
		int readCount = read(fd, command, MAX_CMD);
		close(fd);
		if (readCount > 0) {
			for (int i = 0; i < readCount; ++i) {
				if (command[i] == '\0' || command[i] == '\n') {
					command[i] = ' ';
				}
			}    
		}
		cmd = string(command, readCount);  
	}

	int getPid() {
		return pid;
	}
	int getPpid() {
		return procStat.ppid;
	}
	string getUser() {
		return user;
	}
	int getPriority() {
		return procStat.priority;
	}
	char getState() {
		return procStat.state;
	}
	int getVirtMemory() {
		return procStat.virtMemory;
	}
	int getResMemory() {
		return procStat.resMemory;
	}
	int getThreads() {
		return procStat.threads;
	}
	int getCpu() {
		return cpu;
	}
	int getClockTicks() {
		return clockTicks;
	}
	string getTime() {
		return time;
	}
	string getCmd() {
		return cmd;
	}
	
	void show(bool cmdMode) {		
		if(cmdMode) {
			cout << "\n" << setw(5) << pid << " " << cmd.substr(0, 74);
		} else {
			cout << "\n" 
			<< setw(5) << pid << " "
			<< setw(6) << procStat.ppid << " "
			<< setw(11) << user << " "			
			<< setw(5) << procStat.priority << " "						
			<< setw(4) << procStat.state << " "				
			<< setw(8) << procStat.virtMemory << " "
			<< setw(7) << procStat.resMemory << " "
			<< setw(4) << procStat.threads << " "
			<< setw(4) << cpu << " "	
			<< setw(7) << clockTicks << " "	
			<< setw(9) << time;			
		}		
	}
};

class ProcessesArea {
	static const int areaSize = 17;
	int topIndex;
	bool cmdMode;
	vector<Process> *processes;
public:
	ProcessesArea(vector<Process> *processesPtr) {
		cmdMode = false;
		topIndex = 0;
		processes = processesPtr;
	}
	void up() {
		if (topIndex != 0)
			topIndex--;
	}
	void down() {
		if (topIndex != (int)(*processes).size() - areaSize)
			topIndex++;
	}
	void changeOutputMode() {
		if (cmdMode)
			cmdMode = false;
		else 
			cmdMode	= true;
	}
	void show() {
		if (!cmdMode)
			cout << "\n" 
				<< setw(5) << "PID" << " "
				<< setw(6) << "PPID" << " "
				<< setw(11) << "USER" << " "				
				<< setw(5) << "PRI" << " "							
				<< setw(4) << "STA" << " "					
				<< setw(8) << "VIRTM" << " "
				<< setw(7) << "RESM" << " "
				<< setw(4) << "THR" << " "
				<< setw(4) << "CPU%" << " "	
				<< setw(7) << "TICKS" << " "	
				<< setw(9) << "TIME";
		else
			cout << "\n" 
				<< setw(5) << "PID" << " "
				<< setw(3) << "CMD";
		while ((*processes).size() < topIndex + areaSize)
			topIndex--;
		for (int i = topIndex; i < topIndex + areaSize; i++) {
			(*processes)[i].show(cmdMode);
		}
	}
};

class ProcessManager {
	vector<Process> processes;
	map<int, int> *processesTicks;
	ProcessesArea *processesArea;
	int sortParameterIndex,
		processesCount,
		threadsCount;
	bool pause;
public:
	ProcessManager(){
		processesTicks = new map<int, int>();
		processesArea = new ProcessesArea(&processes);
		sortParameterIndex = 0;
		pause = false;
	}
	void run() {
		int updateCounter = 100;
		while (true) {
			if (updateCounter++ == 100) {
				updateCounter = 0;
				update();				
			}	
			usleep(10000);
			if (kbhit()) {
				switch (getchar()) {
				
				case 'p':		
					if (pause)
						pause = false;
					else
						pause = true;
					break;
				case ']':
					processesArea->up();
					show();	
					break;
				case '[':
					processesArea->down();
					show();	
					break;	
				case '\'':
					processesArea->changeOutputMode();
					show();	
					break;	
				case 'q':	
					exit();					
					return;
					
				case 't':		
					showThreads();
					break;
				case 'c':		
					showCmd();
					break;
				case 's':		
					setSort();
					break;
				case 'k':		
					sendSignal();
					break;							
				case 'f':	
					search();
					break;		
				
				default:
					continue;				
				}				
			}					
		}		
	}
	void exit() {
		processes.clear();
		delete processesTicks;
		delete processesArea;
		system("tput reset");
	}
	void setProcAndThrCount() {
		processesCount = processes.size();
		threadsCount = 0;
		for (vector<Process>::iterator i = processes.begin(); i != processes.end(); i++) {
			threadsCount += i->getThreads();
		}
	}
	void setProcesses() {
		map<int, int> *processesTicksBuf = new map<int, int>();
		processes.clear();		
		struct dirent* dirEntity = NULL ;
		DIR* procDir = NULL ;
		procDir = opendir("/proc/") ;
		while ((dirEntity = readdir(procDir)) != 0) {
			if (dirEntity->d_type == DT_DIR && isNumber(dirEntity->d_name)) {
				map<int,int>::iterator time = processesTicks->find(atoi(dirEntity->d_name));
				if (time != processesTicks->end())
					processes.push_back(Process(dirEntity->d_name, time->second));			
				else
					processes.push_back(Process(dirEntity->d_name, 0));	
				processesTicksBuf->insert(pair<int,int>(atoi(dirEntity->d_name),processes.back().getClockTicks()));		
			}
		}
		closedir(procDir);
		processesTicks->clear();
		processesTicks = processesTicksBuf;		
		
		setProcAndThrCount();
		
		if (sortParameterIndex)
			sort();
	}
	void show() {
		system("tput reset");
		cout << "Process manager                                     ";
		cout << "Processes: " << processesCount << "  Threads: " << threadsCount;
		cout << "\n________________________________________________________________________________"; 
		processesArea->show();
		cout << "\n________________________________________________________________________________"; 
		cout << "\n     p - Pause    |  ] - Up   |  [ - Down  |  ' - OutputMode  |  q - Exit" ;
		cout << "\n     t - Threads  |  c - Cmd  |  s - Sort  |  k - Kill        |  f - Search";
		cout << "\nCommand: ";
	}
	void update() {
		if (!pause)
			setProcesses();
		show();
	}
	void sendSignal() {
		system("tput reset");
		cout << "Send signal\n\nEnter PID: ";
		int pid;
		cin >> pid;
		cout << "\nSignals: ";
		cout << "\n1. SIGKILL";
		cout << "\n2. SIGSTOP";
		cout << "\n3. SIGCONT";
		cout << "\n\nChoice: ";
		int choice, sig;
		cin >> choice;
		switch (choice) {
		case 1:
			sig = SIGKILL;
			break;
		case 2:
			sig = SIGSTOP;
			break;
		case 3:
			sig = SIGCONT;
			break;
		default:
			cout << "\nError. Wrong choice\n";
			getchar();
			getchar();
			return;
		}
		if (kill(pid, sig) == -1) {
			cout << "\nError. Wrong pid\n";
			getchar();
			getchar();
		}	
	}
	void showCmd() {
		system("tput reset");
		cout << "Show full cmd line\n\nEnter PID: ";
		int pid;
		cin >> pid;
		for (vector<Process>::iterator i = processes.begin(); i != processes.end(); i++) {
			if (i->getPid() == pid) {
				cout << "\nCMD:\n" << i->getCmd() << "\n";
				getchar();
				getchar();
				return;
			}
		}
		cout << "\nPid error\n";
		getchar();
		getchar();
		return;
	}
	void showThreads() {
		system("tput reset");
		cout << "Show threads\n\nEnter PID: ";
		int pid;
		cin >> pid;
		cout << "\nThreads: ";
		struct dirent* dirEntity = NULL ;
		DIR* taskDir = NULL ;
		char path[30];
		sprintf(path, "/proc/%d/task", pid);
		taskDir = opendir(path) ;
		if (!taskDir) {
			cout << "\nOpen error\n";
			getchar();
			getchar();
			return;
		}
		while ((dirEntity = readdir(taskDir)) != 0) {
			if (dirEntity->d_type == DT_DIR && isNumber(dirEntity->d_name)) {
				
				cout << "\n" << dirEntity->d_name;
			}
		}
		closedir(taskDir) ;				
		cout << "\n";				
		getchar();
		getchar();
	}
	void search() {
		system("tput reset");
		cout << "Search\n\nParameter: ";
		cout << "\n1.  PID";
		cout << "\n2.  CMD";
		cout << "\n\nChoice: ";
		int parameter;
		cin >> parameter;
		switch (parameter) {
		case 1:
			cout << "\nEnter PID: ";
			int pid;
			cin >> pid;
			for (vector<Process>::iterator i = processes.begin(); i != processes.end(); i++) {
				if (i->getPid() == pid) {
					cout << "\n" 
						<< setw(5) << "PID" << " "
						<< setw(6) << "PPID" << " "
						<< setw(11) << "USER" << " "						
						<< setw(5) << "PRI" << " "									
						<< setw(4) << "STA" << " "							
						<< setw(8) << "VIRTM" << " "
						<< setw(7) << "RESM" << " "
						<< setw(4) << "THR" << " "
						<< setw(4) << "CPU%" << " "	
						<< setw(7) << "TICKS" << " "	
						<< setw(9) << "TIME";
					i->show(false);
					cout << "\n\nCMD\n" << i->getCmd() << "\n";
					getchar();
					getchar();
					return;
				}
			}
			cout << "\nError. Wrong pid\n";
			getchar();
			getchar();
			return;
		case 2:
			cout << "\nEnter string: ";
			string str;
			cin >> str;
			bool isFound = false;
			cout << "\n" 
				<< setw(5) << "PID" << " "
				<< setw(3) << "CMD";
			for (vector<Process>::iterator i = processes.begin(); i != processes.end(); i++) {
				if (i->getCmd().find(str) != string::npos) {
					i->show(true);
					isFound = true;
				}
			}
			if (!isFound) {
				cout << "\n\nNot found\n";
			}
			getchar();
			getchar();
			return;
		}
		cout << "\nError. Wrong choice\n";
		getchar();
		getchar();
		return;
	}
	void setSort() {
		system("tput reset");
		cout << "Sort\n\nParameters: "
		<< "\n1.  PID"
		<< "\n2.  PPID"
		<< "\n3.  USER"
		<< "\n4.  PRI"
		<< "\n5.  STA"
		<< "\n6.  VIRTM"
		<< "\n7.  RESM"
		<< "\n8.  THR"
		<< "\n9.  CPU%"
		<< "\n10. TICKS"
		<< "\n11. TIME"
		<< "\n\nChoice: ";
		cin >> sortParameterIndex;
		sortParameterIndex--;
		if (sortParameterIndex < 0 || sortParameterIndex > 10) {
			cout << "\nError. Wrong parameter\n";
			getchar();
			getchar();
			return;
		}
	}
	bool compareProcesses(Process& process1, Process& process2) {
		switch (sortParameterIndex) {
		case 0:
			return process2.getPid() < process1.getPid();
		case 1:
			return process2.getPpid() < process1.getPpid();
		case 2:
			return process2.getUser().compare(process1.getUser()) < 0;
		case 3:
			return process2.getPriority() < process1.getPriority();
		case 4:
			if (process2.getState() == 'R')
				return true;
			else
				if (process1.getState() == 'R')
					return false;
				else
					return process2.getState() < process1.getState();		
		case 5:
			return process2.getVirtMemory() > process1.getVirtMemory();
		case 6:
			return process2.getResMemory() > process1.getResMemory();
		case 7:
			return process2.getThreads() > process1.getThreads();
		case 8:
			return process2.getCpu() > process1.getCpu();
		case 9:
			return process2.getClockTicks() > process1.getClockTicks();
		case 10:
			return process2.getTime().compare(process1.getTime()) > 0;
		}
	}
	void sort() {
		for (int i = 0; i < processes.size() - 1; i++) 
			for (int j = i + 1; j < processes.size(); j++)
				if (compareProcesses(processes[i], processes[j]))
					swap(processes[i], processes[j]);
	}	
	
	int kbhit(void) {
		struct termios oldt, newt;
		int ch;
		int oldf;
	 
		tcgetattr(STDIN_FILENO, &oldt);
		newt = oldt;
		newt.c_lflag &= ~(ICANON | ECHO);
		tcsetattr(STDIN_FILENO, TCSANOW, &newt);
		oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
		fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
		
		ch = getchar();
	 
		tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
		fcntl(STDIN_FILENO, F_SETFL, oldf);
	 
		if (ch != EOF) {
			ungetc(ch, stdin);
			return ch;
		} 
		return 0;
	}
	bool isNumber(const char* string) {
		for ( ; *string; string++)
			if (*string < '0' || *string > '9')
				return false;
		return true;
	}
};

int main() {
	ProcessManager processManager;
	processManager.run();
	return 0;
}
