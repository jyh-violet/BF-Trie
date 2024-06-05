//
// Created by jyh_2 on 2021/5/14.
//

#ifndef QTREE_TOOLS_H
#define QTREE_TOOLS_H
#define MAX_LOG_SIZE 1000
#define LOG_PATH "log"
#define NO_SEARCH_KEY
#include <stdint.h>
#define MaxThread 128
#ifdef __cplusplus
extern "C"
{
#endif

extern int printLog;
typedef enum LOGLevel{
    InsertLog,
    MiXLog,
    RemoveLog,
    WARN,
    ERROR,
}LOGLevel;

typedef enum DataRegionType {
    Same,
    Random,
    RandomFind,
    Zipf,
    ZipfFind
}DataRegionType;


typedef enum BOOL{
    FALSE,
    TRUE
}BOOL;


void handle_error (int retval);

void initZipfParameter(int n, double zipfianpara);
int zipf();
double zeta(long n, double thetaVal) ;
double zetastatic(long n, double theta) ;

void printArray(int* array, int num);
char *myItoa(int num, char *str);


int BFPRT(int a[], int l, int r, int k);
int Partion(int a[], int l, int r, int p);
int FindMid(int a[], int l, int r);
void InsertSort(int a[], int l, int r);

void vmlog(LOGLevel logLevel, char* fmat, ...);

#ifdef __cplusplus
}
#endif
#endif //QTREE_TOOLS_H
