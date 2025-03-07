import sys
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QTableWidget, QTableWidgetItem, QPushButton,
    QLineEdit, QLabel, QVBoxLayout, QHBoxLayout, QWidget, QStackedWidget
)
from PyQt5.QtCore import Qt


class MyWindow(QMainWindow):
    def __init__(self):
        super(MyWindow, self).__init__()
        self.setWindowTitle("Child Safety App")
        self.setGeometry(400, 400, 400, 400)

        # Stacked Widget for Page Management
        self.stacked_widget = QStackedWidget()
        self.setCentralWidget(self.stacked_widget)

        # Create Pages
        self.main_page = self.create_main_page()
        self.settings_page = self.create_settings_page()

        # Add Pages to Stacked Widget
        self.stacked_widget.addWidget(self.main_page)
        self.stacked_widget.addWidget(self.settings_page)

    def create_main_page(self):
        page = QWidget()
        layout = QVBoxLayout()

        # Table Widget
        self.tableWidget = self.create_table_widget()
        layout.addWidget(self.tableWidget)

        # Settings Button
        settings_button = QPushButton("Settings")
        settings_button.clicked.connect(self.show_settings_page)
        layout.addWidget(settings_button)

        page.setLayout(layout)
        return page

    def create_table_widget(self):
        table_widget = QTableWidget()
        table_widget.setColumnCount(2)
        table_widget.setHorizontalHeaderLabels(["Time", "Prediction Result"])
        table_widget.setEditTriggers(QTableWidget.NoEditTriggers)

        # Load data from file
        self.load_table_data(table_widget)

        return table_widget

    def load_table_data(self, table_widget):
        file_path = "./result.txt"
        try:
            with open(file_path, "r") as file:
                lines = file.readlines()
                table_widget.setRowCount(len(lines))
                for row, line in enumerate(lines):
                    columns = line.strip().split(" ")
                    for col, value in enumerate(columns):
                        item = QTableWidgetItem(value)
                        table_widget.setItem(row, col, item)
        except FileNotFoundError:
            print(f"File not found: {file_path}")
        except Exception as e:
            print(f"An error occurred while loading table data: {e}")

    def create_settings_page(self):
        page = QWidget()
        layout = QVBoxLayout()

        # App Password Field
        self.app_password = QLineEdit()
        self.app_password.setPlaceholderText("App Password")
        self.app_password_warning = QLabel()
        self.app_password_warning.setStyleSheet("color: red;")
        self.app_password.textChanged.connect(self.validate_app_password)
        layout.addLayout(self.create_form_row("App Password:", self.app_password, self.app_password_warning))

        # Parent 1 Phone Field
        self.parent1_phone = QLineEdit()
        self.parent1_phone.setPlaceholderText("Parent 1 Phone")
        self.parent1_phone_warning = QLabel()
        self.parent1_phone_warning.setStyleSheet("color: red;")
        self.parent1_phone.textChanged.connect(lambda: self.validate_phone_number(self.parent1_phone, self.parent1_phone_warning))
        layout.addLayout(self.create_form_row("Parent 1 Phone:", self.parent1_phone, self.parent1_phone_warning))

        # Parent 2 Phone Field
        self.parent2_phone = QLineEdit()
        self.parent2_phone.setPlaceholderText("Parent 2 Phone")
        self.parent2_phone_warning = QLabel()
        self.parent2_phone_warning.setStyleSheet("color: red;")
        self.parent2_phone.textChanged.connect(lambda: self.validate_phone_number(self.parent2_phone, self.parent2_phone_warning))
        layout.addLayout(self.create_form_row("Parent 2 Phone:", self.parent2_phone, self.parent2_phone_warning))

        # Save Button
        self.save_button = QPushButton("Save")
        self.save_button.setStyleSheet("background-color: lightgray;")
        self.save_button.clicked.connect(self.save_settings)
        layout.addWidget(self.save_button)

        # Back Button
        back_button = QPushButton("Back")
        back_button.clicked.connect(self.show_main_page)
        layout.addWidget(back_button, alignment=Qt.AlignTop | Qt.AlignRight)

        page.setLayout(layout)
        return page

    def create_form_row(self, label_text, input_widget, warning_label):
        row = QHBoxLayout()
        label = QLabel(label_text)
        row.addWidget(label)
        row.addWidget(input_widget)
        row.addWidget(warning_label)
        return row

    def show_settings_page(self):
        self.stacked_widget.setCurrentWidget(self.settings_page)

    def show_main_page(self):
        self.stacked_widget.setCurrentWidget(self.main_page)

    def validate_app_password(self):
        password = self.app_password.text()
        if len(password) < 8 or len(password) > 12:
            self.app_password_warning.setText("Password must be 8-12 characters long.")
        else:
            self.app_password_warning.setText("")

    def validate_phone_number(self, phone_field, warning_label):
        phone = phone_field.text()
        if len(phone) != 10 or not phone.isdigit():
            warning_label.setText("Phone number must be exactly 10 digits.")
        elif phone.startswith("0"):
            warning_label.setText("Phone number cannot start with 0.")
        else:
            warning_label.setText("")

    def save_settings(self):
        if not self.app_password_warning.text() and not self.parent1_phone_warning.text() and not self.parent2_phone_warning.text():
            with open("settings.txt", "w") as file:
                file.write(f"App Password: {self.app_password.text()}\n")
                file.write(f"Parent 1 Phone: {self.parent1_phone.text()}\n")
                file.write(f"Parent 2 Phone: {self.parent2_phone.text()}\n")
            self.save_button.setStyleSheet("background-color: green;")
            print("Settings saved!")
        else:
            print("Fix the warnings before saving.")

def window():
    app = QApplication(sys.argv)
    main_window = MyWindow()
    main_window.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    window()
