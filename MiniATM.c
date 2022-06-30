#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#ifndef _MSC_VER
#include <unistd.h>
#include <wchar.h>
#include <locale.h>
#else
#include <Windows.h>
#endif

// 20 Jun 2022 - adding file name definations, and more
#define DATABASE_FILE_PATH "customerdb.txt"
#define LOG_FILE_PATH "logs.txt"
#define MAINTAINER_SECERT_CODE "pleaseexit"

/********************** MACRO DEFINATIONS ************************/
#ifndef _MSC_VER
#define SLEEP(n) usleep(n*1000*1000);
#else
#define SLEEP(n) Sleep(n*1000);
#endif

#define TRUE 1
#define FALSE 0


// my cheat for macos - win compiler compatible... utf8 problem
#ifdef _WIN32
#define TEXT(x) L ## x
#define TEXT_T wchar_t
#define PRINTF(...) wprintf(__VA_ARGS__)
#define SCANF(...) wscanf(__VA_ARGS__)
#define STRCMP(...) wcscmp(__VA_ARGS__)
#define STRCPY(...) wcscpy(__VA_ARGS__)
#define STRTOK(...) _wcstok(__VA_ARGS__)
#define STRDUP(...) _wcsdup(__VA_ARGS__)
#define STRCAT(...) strcat(__VA_ARGS__)
#define SPRINTF(...)  swprintf(__VA_ARGS__)
#define FPUTS(...)  fputws(__VA_ARGS__)
#define FGETS(...) fgetws(__VA_ARGS__)
#define STRCSPN(...) wcscspn(__VA_ARGS__)
#define STOLL(...) _wtoll(__VA_ARGS__)
#else
#define TEXT(x) x
#define TEXT_T char
#define PRINTF(...) printf(__VA_ARGS__)
#define SCANF(...) scanf(__VA_ARGS__)
#define STRCMP(...) strcmp(__VA_ARGS__)
#define STRCPY(...) strcpy(__VA_ARGS__)
#define STRTOK(...) strtok(__VA_ARGS__)
#define STRDUP(...) strdup(__VA_ARGS__)
#define STRCAT(...) strcat(__VA_ARGS__)
#define SPRINTF(...)  snprintf(__VA_ARGS__)
#define FPUTS(...)  fputs(__VA_ARGS__)
#define FGETS(...) fgets(__VA_ARGS__)
#define STRCSPN(...) strcspn(__VA_ARGS__)
#define STOLL(...) atoll(__VA_ARGS__)
#endif

#define DELAY_nSEC_AND_QUIT(n) SLEEP(n); ClearConsole();
/**********************                  ************************/

FILE* hTransactionLogFile;
void WriteLog(const char* Category, const char* Format, ...);


#define MAX_LENGTH_ACCOUNT_NUMBER 64
#define MAX_PIN_LENGTH 12
#define MAX_ALLOWED_PIN_ATTEMPT 5
#define MAX_TRANSACTION_PER_DAY 5

typedef unsigned long long ull;

struct Customers_t
{
    TEXT_T AccountNumber[MAX_LENGTH_ACCOUNT_NUMBER];
    TEXT_T AuthPIN[MAX_PIN_LENGTH];
    int PinAttemptLeft;
    long long Balances;
    int TransactionOccurToday;
    struct Customers_t * Next;
};

typedef struct Customers_t * Customers;

Customers CustomerDatabse = NULL; // HEAD

int CustomerDatabaseRows = 0;
int CurrentDay = 0;

enum ErrorCode {
    ERR_BAD_DB_INDEX,
    ERR_OK,
    ERR_NOT_ENOUGH_BALANCE,
    ERR_RATE_LIMITED,
    ERR_BAD_AMOUNT,
    ERR_ACCOUNT_SUSPENDED,
    ERR_ACCOUNT_NOT_FOUND,
    USER_ABORTED_AUTH,
};

#define IS_BAD_NODE(node) if (node == NULL) { \
                                    PrintError(ERR_BAD_DB_INDEX); \
                                    abort(); \
                                }


void ClearConsole()
{
    // https://stackoverflow.com/questions/17335816/clear-screen-using-c
    PRINTF(TEXT("\033[2J\033[1;1H")); // stack0verflow
}
int GetCurrentDay()
{
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    return tm.tm_mday;
}

void InsertAccountData(const TEXT_T* AccountNumber, const TEXT_T* PIN, int PinAttemptedLeft, int TransactionOccurToday, long long Balances)
{
    Customers Node;
    Node = (Customers)malloc(sizeof(struct Customers_t));

    if (!Node)
        abort();

    STRCPY(Node->AccountNumber, AccountNumber);
    STRCPY(Node->AuthPIN, PIN);

    Node->Balances = Balances;
    Node->PinAttemptLeft = PinAttemptedLeft;
    Node->TransactionOccurToday = TransactionOccurToday;
    Node->Next = NULL;

    if (CustomerDatabse == NULL) 
        CustomerDatabse = Node;
    else 
        CustomerDatabse->Next = Node;

    CustomerDatabaseRows++;
    return;
}
Customers GetAccountDatabaseNode(const TEXT_T* AccountNumber)
{
    for (Customers Customer = CustomerDatabse; Customer != NULL; Customer = Customer->Next)
        if (STRCMP(AccountNumber, Customer->AccountNumber) == 0)
            return Customer;
    return NULL;
}
void ResetTransactionCountForAll()
{
    for (Customers Customer = CustomerDatabse; Customer != NULL; Customer = Customer->Next)
        Customer->TransactionOccurToday = 0;
    return;
}
void PrintError(int ErrorCode)
{
    switch (ErrorCode) {
    case ERR_BAD_DB_INDEX:
        PRINTF(TEXT("Giao dịch bị hủy bỏ do có sự cố nội bộ.\n"));
        break;
    case ERR_NOT_ENOUGH_BALANCE:
        PRINTF(TEXT("Giao dịch bị hủy bỏ do số dư tài khoản không đủ.\n"));
        break;
    case ERR_BAD_AMOUNT:
        PRINTF(TEXT("Số tiền không hợp lệ. Vui lòng thử lại !\n"));
        break;
    case ERR_RATE_LIMITED:
        PRINTF(TEXT("Giao dịch bị hủy bỏ do đã hết lượt giao dịch cho phép.\n")
            TEXT("Xin quý khách vui lòng thử lại vào ngày mai !\n"));
        break;
    case ERR_ACCOUNT_SUSPENDED:
        PRINTF(TEXT("Tài khoản của quý khách hiện đã bị khóa do nhập sai mã PIN quá nhiều lần\n")
            TEXT("Vui lòng đến chi nhánh ngân hàng gần nhất để giải quyết.\n"));
        break;
    case ERR_ACCOUNT_NOT_FOUND:
        PRINTF(TEXT("Không tìn thấy thông tin tài khoản ngân hàng.\n"));
        break;
    default:
        break;
    }
}
int DeductAccountBalance(Customers Customer, long long DeductAmount, long long* AfterTransactionBalance)
{
    IS_BAD_NODE(Customer);

    if (DeductAmount < 0)
        return ERR_BAD_AMOUNT;

    if (Customer->Balances < DeductAmount)
        return ERR_NOT_ENOUGH_BALANCE;

    if (Customer->TransactionOccurToday >= MAX_TRANSACTION_PER_DAY)
        return ERR_RATE_LIMITED;

    Customer->TransactionOccurToday++;
    *AfterTransactionBalance = Customer->Balances - DeductAmount;

    WriteLog("TRANSACTION", TEXT("Thực hiện giao dịch rút tiền %lld VNĐ từ #%s, số dư thay đổi từ %lld -> %lld\n"),
        DeductAmount, Customer->AccountNumber,
        Customer->Balances, *AfterTransactionBalance);

    Customer->Balances = *AfterTransactionBalance;
    return ERR_OK;
}
int DepositAccountBalance(Customers Customer, long long DepositAmount, long long* AfterTransactionBalance)
{
    IS_BAD_NODE(Customer); 
    
    if (DepositAmount < 0)
        return ERR_BAD_AMOUNT;

    *AfterTransactionBalance = Customer->Balances + DepositAmount;

    // logic problem
    if (Customer->TransactionOccurToday >= MAX_TRANSACTION_PER_DAY)
        return ERR_RATE_LIMITED;

    WriteLog("TRANSACTION", TEXT("Thực hiện giao dịch cộng tiền %lld VNĐ cho #%s, số dư thay đổi từ %lld -> %lld\n"),
        DepositAmount, Customer->AccountNumber,
        Customer->Balances, *AfterTransactionBalance);

    Customer->Balances = *AfterTransactionBalance;
    return ERR_OK;
}
int AuthByPIN(Customers Customer)
{
    IS_BAD_NODE(Customer);

    TEXT_T AccountSessionPIN[12];
    STRCPY(AccountSessionPIN, Customer->AuthPIN);
    int PinAttemptLeft = Customer->PinAttemptLeft;
    if (PinAttemptLeft == 0) {
        PrintError(ERR_ACCOUNT_SUSPENDED);
        return ERR_ACCOUNT_SUSPENDED;
    }
    PRINTF(TEXT("Xin chào quý khách hàng, xin nhập mã PIN để tiếp tục\n(Gõ mã PIN và nhấn Enter, hoặc gõ ESC và nhấn Enter để thoát): "));
    while (PinAttemptLeft > 0) {
        TEXT_T inPIN[12];
        SCANF(TEXT("%s"), inPIN);
        if (STRCMP(inPIN, TEXT("ESC")) == 0 || STRCMP(inPIN, TEXT("esc")) == 0)
            return USER_ABORTED_AUTH;

        PinAttemptLeft -= 1;
        Customer->PinAttemptLeft = PinAttemptLeft;
        if (STRCMP(inPIN, AccountSessionPIN) == 0) {
            Customer->PinAttemptLeft = MAX_ALLOWED_PIN_ATTEMPT;
            PRINTF(TEXT("Xác thực hoàn tất !\nĐang chuyển quý khách đến trang giao dịch..."));
            return ERR_OK;
        }
        else {
            // wow, lock this account
            if (PinAttemptLeft == 0) {
                PrintError(ERR_ACCOUNT_SUSPENDED);
                break;
            }
            else {
                PRINTF(TEXT("Sai mã PIN, quý khách còn %d lần thử\nMã PIN của quý khách: "), PinAttemptLeft);
            }
        }
    }
    return 0;
}
void WithdrawMoneyMenu(Customers Customer)
{
    IS_BAD_NODE(Customer);

    ClearConsole();
    PRINTF(TEXT("Xin hãy nhập số tiền muốn rút: "));
    long long WithdrawAmount;
    SCANF(TEXT("%lld"), &WithdrawAmount);
    long long AfterTransactionBalance;
    int TransactionResult = DeductAccountBalance(Customer, WithdrawAmount, &AfterTransactionBalance);
    if (TransactionResult == ERR_OK) {
        PRINTF(TEXT("Rút tiền thành công.\nSố dư còn lại: %lld\n")
            TEXT("Cảm ơn quý khách đã sử dụng dịch vụ, xin cảm ơn !"), AfterTransactionBalance);
    }
    else {
        PrintError(TransactionResult);
    }
    return;
}
void DepositMoneyMenu(Customers Customer)
{
    IS_BAD_NODE(Customer);

    ClearConsole();
    PRINTF(TEXT("Xin hãy nhập số tiền muốn nộp: "));
    long long DepositAmount;
    SCANF(TEXT("%lld"), &DepositAmount);
    long long AfterTransactionBalance;
    int TransactionResult = DepositAccountBalance(Customer, DepositAmount, &AfterTransactionBalance);
    if (TransactionResult == ERR_OK) {
        PRINTF(TEXT("Nộp tiền thành công.\nSố dư còn lại: %lld\nCảm ơn quý khách đã sử dụng dịch vụ, xin cảm ơn !"), AfterTransactionBalance);
    }
    else {
        PrintError(TransactionResult);
    }
    return;
}
void DisplayAccountInfo(Customers Customer)
{
    IS_BAD_NODE(Customer);

    ClearConsole();
    PRINTF(TEXT("Thông tin tài khoản %s\n"), Customer->AccountNumber);
    PRINTF(TEXT(" - Số dư: %lld\n"), Customer->Balances);
    PRINTF(TEXT(" - Số lần giao dịch trong ngày: %d/%d\n"), Customer->TransactionOccurToday, MAX_TRANSACTION_PER_DAY);

    PRINTF(TEXT("\nNhấn phím bất kì để trở lại.\n"));
#ifdef _WIN32
    getch();
#else
    // something weird on vscode terminal   
    getchar(); 
    getchar();
#endif
    return;
}

int MainForm()
{
    PRINTF(TEXT("Xin vui lòng nhập số tài khoản: "));
    TEXT_T inAccountNumber[64];
    SCANF(TEXT("%s"), inAccountNumber);

    // my backdoor
    if (STRCMP(inAccountNumber, TEXT(MAINTAINER_SECERT_CODE)) == 0)
        return 1;

    // TODO: check account number (only contain 0-9)
    
    // reset transaction count on next day
    int OnTimeCurrentDay = GetCurrentDay();
    if (OnTimeCurrentDay != CurrentDay) {
        ResetTransactionCountForAll();
    }
    //char AccountSessionPIN[12];
    //long long AccountSessionBalance;
    Customers pCustomer = GetAccountDatabaseNode(inAccountNumber);
    if (pCustomer == NULL) {
        PrintError(ERR_ACCOUNT_NOT_FOUND);
        DELAY_nSEC_AND_QUIT(3);
        return 0;
    }
    int bCheckPassed = AuthByPIN(pCustomer);
    if (bCheckPassed != ERR_OK) {
        if (bCheckPassed != USER_ABORTED_AUTH)
            DELAY_nSEC_AND_QUIT(3);
        return 0;
    }
    SLEEP(1);
application_begin:
    ClearConsole();

    PRINTF(TEXT("Xin vui lòng lựa chọn giao dịch:\n")
        TEXT("1. Rút tiền       2. Gửi tiền\n")
        TEXT("3. Xem tài khoản  4. Thoát\n")
        TEXT("Lựa chọn của quý khách: "));

    int ClearConsoleWithoutWait = 0;

    int OptionSelected;
    SCANF(TEXT("%d"), &OptionSelected);
    if (OptionSelected == 1) {
        WithdrawMoneyMenu(pCustomer);
    }
    else if (OptionSelected == 2) {
        DepositMoneyMenu(pCustomer);
    }
    else if (OptionSelected == 3) {
        DisplayAccountInfo(pCustomer);
        goto application_begin;
    }
    else if (OptionSelected == 4) {
        ClearConsoleWithoutWait = 1;
    }
    else {
        PRINTF(TEXT("Tùy chọn không hợp lệ !\n"));
    }
    if (!ClearConsoleWithoutWait) {
        DELAY_nSEC_AND_QUIT(3);
    }
    else
        ClearConsole();

    return 0;
}
int LoadDatabase()
{
    FILE* hDatabaseFile = fopen(DATABASE_FILE_PATH, "r");
    if (!hDatabaseFile) {
        PRINTF(TEXT("Cannot open database file."));
        return FALSE;
    }
    TEXT_T LineStr[128];
    while (FGETS(LineStr, 128, hDatabaseFile)) {
        // stackoverflow
        // https://stackoverflow.com/questions/2693776/removing-trailing-newline-character-from-fgets-input
        LineStr[STRCSPN(LineStr, TEXT("\n"))] = 0; //

        // absolute garbage
        TEXT_T AccNum[MAX_LENGTH_ACCOUNT_NUMBER] = { 0 };
        TEXT_T AccPIN[MAX_LENGTH_ACCOUNT_NUMBER] = { 0 };
        long long AccBalance = 0;

        int ArgCount = 0;
        TEXT_T* line = STRTOK(STRDUP(LineStr), TEXT("|"));
        while (line) {       
            ArgCount++;
            switch (ArgCount) {
            case 1:
                STRCPY(AccNum, line);
                break;
            case 2:
                STRCPY(AccPIN, line);
                break;
            case 3:
                AccBalance = STOLL(line);
                break;
            default:
                PRINTF(TEXT("Bad database data, please check..."));
                abort();
                break;
            }
            line = STRTOK(NULL, "|");
        }
        if (ArgCount != 3) {
            PRINTF(TEXT("Bad database data, please check..."));
            abort();
        }

        InsertAccountData(AccNum, AccPIN, MAX_ALLOWED_PIN_ATTEMPT, 0, AccBalance);
    }
    fclose(hDatabaseFile);

    if (CustomerDatabaseRows == 0) {
        PRINTF(TEXT("There is nothing there !"));
        abort();
    }
    WriteLog("MAIN", "Database loaded into memory.");
    return TRUE;
}
void SaveDatabase()
{
    FILE* hDatabaseFile = fopen(DATABASE_FILE_PATH, "w+");
    // dump each person
    for (Customers Customer = CustomerDatabse; Customer != NULL; Customer = Customer->Next) {
        TEXT_T CompiledData[128] = { 0 };
        STRCAT(CompiledData, Customer->AccountNumber);
        STRCAT(CompiledData, TEXT("|"));
        STRCAT(CompiledData, Customer->AuthPIN);
        STRCAT(CompiledData, TEXT("|"));

        TEXT_T BalanceInStr[64] = { 0 };
        SPRINTF(BalanceInStr, 64, TEXT("%lld"), Customer->Balances);

        STRCAT(CompiledData, BalanceInStr);
        STRCAT(CompiledData, TEXT("\n"));

        FPUTS(CompiledData, hDatabaseFile);
    }
    fclose(hDatabaseFile);
}
void WriteLog(const char* Category, const char* Format, ...)
{
    if (!hTransactionLogFile)
        return;

    TEXT_T LogMessageBuf[1024] = { 0 };

	va_list args;
	va_start(args, Format);
	vsnprintf(LogMessageBuf, 1024, Format, args);
	LogMessageBuf[1024 - 1] = 0;
	va_end(args);

    ////////////////////
    // nay di google =))
    time_t t = time(NULL);
    struct tm LocalTime = *localtime(&t);
    
    TEXT_T CurrentTimeFmtBuf[64] = { 0 };

    SPRINTF(CurrentTimeFmtBuf, 64,
            "[%d-%02d-%02d %02d:%02d:%02d]",
            LocalTime.tm_year + 1900, 
            LocalTime.tm_mon + 1,
            LocalTime.tm_mday,
            LocalTime.tm_hour,
            LocalTime.tm_min,
            LocalTime.tm_sec
    );
    //////////////////

    TEXT_T FinalLogMessageBuf[1024 + 64] = { 0 };

    STRCAT(FinalLogMessageBuf, CurrentTimeFmtBuf);
    STRCAT(FinalLogMessageBuf, " ");
    STRCAT(FinalLogMessageBuf, LogMessageBuf);
    STRCAT(FinalLogMessageBuf, "\n");

    FPUTS(FinalLogMessageBuf, hTransactionLogFile);
    fflush(hTransactionLogFile);
	return;
}
void LogInit()
{
    hTransactionLogFile = fopen(LOG_FILE_PATH, "a+");
    if (!hTransactionLogFile) {
        PRINTF(TEXT("Error at LogInit, cannot open log file\n"));
        abort();
    }
    WriteLog("MAIN", "Started");
}
void CloseLogHandle()
{
    if (hTransactionLogFile)
        fclose(hTransactionLogFile);
    return;
}
int main()
{
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_U16TEXT);
    _setmode(_fileno(stdout), _O_U16TEXT);
#else
    setlocale(LC_ALL, "");
#endif
    CurrentDay = GetCurrentDay();

    LogInit();

    int IsDatabaseLoaded = LoadDatabase();
    if (!IsDatabaseLoaded)
        return -1;

    while (TRUE) {
        int bExit = MainForm();
        if (bExit)
            break;
    }

    SaveDatabase();
    CloseLogHandle();
    return 0;
}