Directly run the main.py file:
python main.py

** Ensure the resulttable.txt file to be in the same directory with main.py<br>
** settings.txt file is created automatically if it doesn't exist. If it exists, overwritten.


** UI Project Run Steps
1- Press Ctrl + Shift + P
2- Type "ESP-IDF Terminal" then press Enter
3- Change directory to DashboardApplication
4- run: idf.py build
5- After build, idf.py flash

Note: The current sdkconfig file is generated when the esp32 is not connected to computer. Before proceeding, make sure you configure COM port and esp32s3. To do so, you can use "idf.py set-target esp32s3". Also make sure to check for COM port.

Note: If error occurs while building immediately,
Change the paths in the dependencies.lock file with your project path.
Example:
Your local directory for DashboardApplication directory is "C:\Users\<username>\Desktop\Capstone-Project\DashboardApplication"
Then edit the path entries in the above specified dependency file and replace the part of paths up-to including DashboardApplication part with your local directory path.
