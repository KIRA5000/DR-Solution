# cpp-sql

### Build
 - Download and install latest cmake
 - Fork the main repo
 - Clone the forked repo under a folder say c:\cpp-rct
 - From inside a VS 2019 x64 command prompt:

```sh
cd c:\cpp-sql
md build
cd build
cmake ..
cmake --build . --config Release
```

### component selection
 - Database Name specified by the user is the component selected for backup

### referred material
 - https://www.dell.com/community/NetWorker/MSSQL-VDI-backup-vs-MSSQL-VSS-backup/td-p/6902832
 - https://www.dell.com/community/Microsoft/SQL-backups-VSS-or-VDI/td-p/7097889
 - https://sqlbak.com/academy/full-backup
 - https://docs.microsoft.com/en-us/sql/relational-databases/backup-restore/tail-log-backups-sql-server?view=sql-server-ver15
 - https://docs.microsoft.com/en-us/sql/t-sql/statements/backup-transact-sql?view=sql-server-ver15
 - https://patents.justia.com/patent/10108497
 - https://www.sqlbackuprestore.com/3rdpartybackupapps_vdi.htm
 - https://www.dell.com/community/NetWorker/The-Overview-of-NMM-SQL-Server-VDI-Backup-and-Restore/td-p/7137435
 - https://www.sqlshack.com/point-in-time-recovery-with-sql-server/
 - https://docs.oracle.com/javadb/10.8.3.0/adminguide/cadminrollforward.    html#:~:text=Roll%2Dforward%20recovery%20recovers%20the,in%20the%20database%20log%20directory
 - https://www.sqlskills.com/blogs/paul/multiple-log-files-and-why-theyre-bad/
 - https://www.sqlshack.com/sql-server-transaction-log-administration-best-practices/