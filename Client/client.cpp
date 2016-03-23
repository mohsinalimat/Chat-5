#include "client.h"
#include "ui_client.h"
#include <QDebug>
#include <QPixmap>
#include <QImage>
#include <QIcon>
#include <QFileDialog>
#include <QString>
#include <QPainter>
#include <time.h>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>

QString gl_fname; //Поиск человека

//СДЕЛАТЬ:
// Передачу аватарки, а не рандомная загрузка из ресурсов
// Создание группового чата
// Смайлики доделать по нормальному
// Информация о друге.
// Повторное добавление удаленного друга



//СДЕЛАНО:
// Завтра доделать разделение на приватные беседы.  -- Сделано
// Стэк Списков, вектора на адреса списков.         -- Сделано
// Попробовать добавить 1-2 языка..                 -- Сделано (Русский)
// Изменил верхнюю панель, завтра прикручу)         -- Сделано
// Доделать обновление статуса(последнего сообщения) под ником у друзей                         --Сделано
// При добавлении в список друзей просматривать Пол, и в зависимости от этого ставить аватар.   -- Сделано
// Отправку сообщения по текущему диалогу, а не по команде msg User     --Сделано
// Добавлены уведомления в углу экрана.
// Дисконнект из-за доп.сокетов и соединений при авторизации. (Правильно прикрутить закрытие сокета) -- Сделано
// СОХРАНЕНИЕ ПОЛЬЗОВАТЕЛЕЙ Друг у Друга (Через SQLite) --Сделано
// Сохранение и загрузка личных сообщений(включая файлы, размер, и отдельную иконку). -- Сделано
// Демо вариант.
// Возможно исправить делегата, сделав сообщения пузырьком. -- Сделано
// Устойчивая отправка файлов до клииента                   -- СДЕЛАНО
// RSA - Осталась отправка. Шифрование сделано.             -- Сделано
// MD5+Solt Хэширование - Сделано
// Поиск по контактам.  - Сделано
// Удаление контакта и удаление истории переписки - Добавлено



Client::Client(QWidget *parent) : QMainWindow(parent), download_path("(C:/...)"), personDates(false),ui(new Ui::Client)
{
    srand((time(NULL)));
    ui->setupUi(this);
    this->setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::CustomizeWindowHint);

    frameEmoji      = new EmojiFrame();
    emojiMan        = new EmojiManager();
    notice          = new Notification();
    reg_window      = new registration();
    trayIcon        = new TrayIcon(this);
    stackchat       = new QStackedWidget;
    trayIconMenu    = new QMenu(this);
    tcpSocket       = new QTcpSocket(this);
    rsaCrypt        = new RSACrypt;

    QColor defaulcolor(Qt::white);
    colorchat = defaulcolor.name();
    ui->RB_sendEnter->setChecked(true);
    ui->userList->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->stackedWidget_2->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->userList->setItemDelegate(new UserListDelegate(ui->userList));
    ui->search_list->setItemDelegate(new UserListDelegate(ui->search_list));
    ui->label_6->setText(QDir::homePath() + "/Whisper/");
    ui->widget_2->hide();
    ui->search_list->hide();
    ui->glass_button->raise();
    ui->glass_button->hide();
    nextBlockSize = 0;
    FriendCount = 0;

    trayIconMenu->addAction(ui->actionShowHideWindow);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(ui->actionExit);
    trayIcon->setContextMenu(trayIconMenu);
    trayIcon->hide();

    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));
    connect(reg_window, SIGNAL(sendData(QString, QString, QString)), this, SLOT(recieveData(QString, QString, QString)));
    connect(frameEmoji, SIGNAL(sendEmoji(QString)), this, SLOT(insertEmoticon(QString)));

    connect(tcpSocket, SIGNAL(readyRead()), this, SLOT(getMessage()));
    connect(tcpSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(show_Error(QAbstractSocket::SocketError)));
    connect(tcpSocket, SIGNAL(connected()), this, SLOT(send_personal_data()));
    connect(tcpSocket, SIGNAL(disconnected()), this, SLOT(onDisconnect()));

    connect(ui->userSetting_button, SIGNAL(clicked()), this, SLOT(on_userSetting_clicked()));
    connect(ui->close_setting_button_2, SIGNAL(clicked()), this, SLOT(on_close_setting_button_clicked()));
    connect(ui->userList_3, SIGNAL(itemClicked(QListWidgetItem*)), SLOT(whisperOnClick(QListWidgetItem*)));
    connect(ui->userList, SIGNAL(itemDoubleClicked(QListWidgetItem*)), SLOT(whisperOnClickUsers(QListWidgetItem*)));
    connect(ui->userList, SIGNAL(itemClicked(QListWidgetItem*)), SLOT(whisperOnClickSelectUsers(QListWidgetItem*)));
    connect(ui->actionExit, SIGNAL(triggered()), this, SLOT(close()));
    connect(ui->stackedWidget_2, SIGNAL(customContextMenuRequested(const QPoint &)), SLOT(showContextMenuForChat(const QPoint &)));
    connect(ui->userList, SIGNAL(customContextMenuRequested(const QPoint &)), SLOT(showContextMenuForWidget(const QPoint &)));

}

void Client::keyReleaseEvent(QKeyEvent *event)
{
    if(ui->RB_sendEnter->isChecked())
    {
        switch(event->key()) {
        case Qt::Key_Escape:
            // Обработка нажатия Esc
            break;
        case Qt::Key_Enter:
        case Qt::Key_Return:
            if(ui->widget_2->isHidden() && ui->RB_sendEnter->isChecked())
                on_sendMessage_clicked();
            break;
            // Обработчики других клавиш
        }
    }

    if(ui->RB_send_CEnter->isChecked())
        if(event->modifiers()==Qt::ControlModifier)
            if(event->key() == Qt::Key_Return)
                if(ui->widget_2->isHidden() && ui->RB_send_CEnter->isChecked())
                    on_sendMessage_clicked();
}

void Client::recieveData(QString str, QString pas, QString pubKey)
{
    if(str!="" && pas!="")
    {
        name = str;
        name.replace(" ", "_");
        ui->usernameEdit->setText(name);

        QDir keyDir = QDir::homePath() + "/Whisper Close Key/";
        QLineEdit m_ptxtMask("*.txt");
        QStringList listFiles = keyDir.entryList(m_ptxtMask.text().split(" "), QDir::Files);
        myPublicKey = pubKey;

        for(int i=0; i<listFiles.size(); i++)
            if(listFiles.at(i)==name+".txt")
            {
                QFile myKey(QDir::homePath() + "/Whisper Close Key/" + name + ".txt");
                myKey.open(QIODevice::ReadOnly);
                myPrivateKey = myKey.readAll();
                break;
            }
        qDebug() << "Закрытый ключ: " << myPrivateKey;
        QString hostname = "127.0.0.1";
        quint32 port = 55155;
        tcpSocket->abort();
        tcpSocket->connectToHost(hostname, port);

        this->show();
        QPropertyAnimation* animation = new QPropertyAnimation(this, "windowOpacity");
        animation->setDuration(1500);
        animation->setStartValue(0);
        animation->setEndValue(1);
        animation->start();

        trayIcon->show();
    }
}

void Client::on_sendMessage_clicked()
{
    QRegExp rx("<a [^>]*name=\"([^\"]+)\"[^>]*>");      // Регулярные выражения для парсинга смайликов в сообщении
    QString str = ui->editText->text();                 // Получение сообщения для отправки
    QString message = str;


    QStringList list;
    QString encodemsg;

    list.clear();
    int pos = 0;

    while ((pos = rx.indexIn(str, pos)) != -1) {
        list << rx.cap(1);
        pos += rx.matchedLength();
    }

    for (int i = 0; i < list.count(); i++) {
        QString emojiNumber = list.at(i);
        QString searchTag = QString("<a name=\"%1\"></a>").arg(emojiNumber);
        QString replTag = emojiMan->getEmojiSymbolFromNumber(emojiNumber);
        str = str.replace(searchTag, replTag);
    }

    QTextDocument doc;
    doc.setHtml(ui->textEdit1->toPlainText());
    doc.toPlainText();

    ui->editText->clear();
    blockSize = 0;

    if (!message.isEmpty() && vec.size()!=0)              //Отправка сообщений
    {
        if(ui->ChBox_PSound->isChecked())
            QSound::play(":/new/prefix1/Resource/to.wav");

        QString RecName = vec.at(ui->userList->currentRow())->data(Qt::DisplayRole).toString();
        QStringList lst = myPublicKey.split(" ", QString::SkipEmptyParts);
        QString MyMsg = rsaCrypt->encodeText(message, lst.at(0).toInt(), lst.at(1).toInt());

        for(int i=0; i<pubFriendKey.size(); i++)
        {
            if(RecName==pubFriendKey.at(i).first)
            {

                QString user_key = pubFriendKey.at(i).second;
                qDebug() << "Ключ друга: " << pubFriendKey.at(i).first << user_key;
                QStringList _key = user_key.split(" ", QString::SkipEmptyParts);
                encodemsg = rsaCrypt->encodeText(message, _key.at(0).toInt(), _key.at(1).toInt());
                qDebug() << "Отправил: " << encodemsg;
                break;
            }
        }

        QString new_mes = "/msg " + vec.at(ui->userList->currentRow())->data(Qt::DisplayRole).toString() + " " + encodemsg;
        sendUserCommand(new_mes, MyMsg);
    }
}

void Client::insertEmoticon(QString symbol)
{
    QTextCursor cursor(ui->textEdit1->textCursor());

    if (!cursor.isNull()) {
        cursor.insertHtml(symbol);
    }

    QTextCursor cursor2(ui->textEdit1->document()->find(symbol));

    QString emojiNumber = emojiMan->getEmojiNumberFromSymbol(symbol);
    QString binDir = QCoreApplication::applicationDirPath();
    QString dataDir = binDir;
    dataDir = QDir::cleanPath(dataDir + "/");

    QDir iconsLocation(dataDir + "/data/icons");


    QString s = QString("%1/x-%2.png")
            .arg(iconsLocation.absolutePath())
            .arg(emojiNumber);

    if (!cursor2.isNull()) {
        QString imgTag = QString("<img src=\"%1\" id=\"%2\" />")
                .arg(s)
                .arg(emojiNumber);

        cursor2.insertHtml(imgTag);
    }
}

void Client::AddUser_Chat(QString _username, QString _sex, QList<QPair<QString, QString>> lst, int count)
{
    int rand_avatar;
    QString sex = _sex;

    // В зависимости от пола рандомно устаналиваю аватар из списка.

    if(sex=="Man")
        rand_avatar = rand()%18+1;
    else if(sex=="Woman")
        rand_avatar = rand()%13+19;
    else if(sex=="Unknown")
        rand_avatar = rand()%20+32;

    QIcon pic(":/Avatars/Resource/Avatars/"+QString::number(rand_avatar)+".jpg");


    // Проверка на повторное добавление человека в список - выходим.
    if (!vec.empty())
        for(int i=0; i<vec.size(); i++)
            if(vec.at(i)->data(Qt::DisplayRole)==_username)
                return;

    //Создание Списка на каждую страницу стека, для отдельного отображения чат переписки и через делегат управляю свойством Item'a

    QListWidgetItem *item = new QListWidgetItem();
    QListWidget *chatlist = new QListWidget();
    chatlist->setItemDelegate(new ChatListDelegate(chatlist, colorchat));
    ui->stackedWidget_2->addWidget(chatlist);

    // Добавление нового пользователя через Поиск.
    if (count==-1 || count == -2)
    {
        item->setData(Qt::DisplayRole, _username);
        item->setData(Qt::ToolTipRole, QDateTime::currentDateTime().toString("dd.MM.yy hh:mm"));
        if (count==-2)
        {   QListWidgetItem *item2 = new QListWidgetItem();
            item->setData(Qt::UserRole + 1, "You added a user");
            item2->setData(Qt::UserRole + 1, "FROM");
            item2->setData(Qt::DisplayRole, "Hey, let me add you to the list of contacts?");
            item2->setData(Qt::ToolTipRole, QDateTime::currentDateTime().toString("dd.MM.yy hh:mm"));
            chatlist->addItem(item2);
        }
        else
            item->setData(Qt::UserRole + 1, "New User");
        item->setData(Qt::DecorationRole, pic);

        vec.push_back(item);
        chatvec.push_back(chatlist);
        ui->userList->addItem(item);
        ui->userList->scrollToBottom();

        if(count==-2)
        {
            ui->textBrowser->hide();
            ui->stackedWidget_2->show();
            ui->stackedWidget_2->setCurrentIndex(chatvec.size()-1);
            ui->userList->clearSelection();
            vec.at(vec.size()-1)->setSelected(true);
            whisperOnClickUsers(vec.at(vec.size()-1));
        }
        return;
    }

    // Добавление пользователя(друга) из БД
    item->setData(Qt::DisplayRole, _username);
    item->setData(Qt::ToolTipRole, QDateTime::currentDateTime().toString("dd.MM.yy hh:mm"));
    item->setData(Qt::DecorationRole, pic);
    vec.push_back(item);
    chatvec.push_back(chatlist);

    // Загрузка Вашей переписки.
    for(int i=0; i<lst.size(); i++)
    {
        QListWidgetItem *item2 = new QListWidgetItem();
        QString msg = lst.at(i).first;
        QString timeStr = (lst.at(i).first);
        timeStr.remove(14, lst.at(i).first.size());

        msg.remove(0,14);
        QString TFile = msg.left(4);

        if(lst.at(i).second.left(1) == "F")
        {
            QStringList lstfrom = myPrivateKey.split(" ", QString::SkipEmptyParts);

            if(msg.left(1)=="F")
            {

            }
            else
                msg = rsaCrypt->decodeText(msg, lstfrom.at(0).toInt(), lstfrom.at(1).toInt());

            if(TFile=="FILE")
            {
                msg.remove(0,5);
                QString fsize = lst.at(i).second;
                QIcon pic(":/new/prefix1/Resource/sendfiles.png");
                item2->setData(Qt::ToolTipRole, fsize.remove(0,5) +  "  " +timeStr);
                item2->setData(Qt::UserRole + 1, "FROMF");
                item2->setData(Qt::DecorationRole, pic);
                item->setData(Qt::UserRole + 1, "File: " + msg);
            }
            else
            {
                item2->setData(Qt::UserRole + 1, "FROM");
                item2->setData(Qt::ToolTipRole, timeStr);
                item->setData(Qt::UserRole + 1, msg);
            }

            item2->setData(Qt::DisplayRole, msg);
            item->setData(Qt::ToolTipRole, timeStr);

        }
        else if (lst.at(i).second.left(1) == "T")
        {


            if(msg.left(1)=="F")
            {

            }
            else{
                QStringList _lst = myPrivateKey.split(" ", QString::SkipEmptyParts);
                msg = rsaCrypt->decodeText(msg, _lst.at(0).toInt(), _lst.at(1).toInt());
            }


            if(TFile=="FILE")
            {
                msg.remove(0,5);
                QString fsize = lst.at(i).second;
                QIcon pic(":/new/prefix1/Resource/sendfiles.png");
                item2->setData(Qt::ToolTipRole, fsize.remove(0,3) + "  " + timeStr);
                item2->setData(Qt::UserRole + 1, "TOF");
                item2->setData(Qt::DecorationRole, pic);
            }
            else
            {
                item2->setData(Qt::UserRole + 1, "TO");
                item2->setData(Qt::ToolTipRole, timeStr);
            }

            item2->setData(Qt::DisplayRole, msg);
        }

        chatvec.at(count)->addItem(item2);
    }
    chatvec.at(count)->scrollToBottom();
    ui->userList->clearSelection();
    ui->stackedWidget_2->hide();
    ui->userList->addItem(item);

}

void Client::getMessage()
{
    QString message;
    QDataStream in(tcpSocket);
    in.setVersion(QDataStream::Qt_5_4);

    if (!nextBlockSize) {
        if (quint32(tcpSocket->bytesAvailable()) < sizeof(quint32)) {
            return;
        }
        in >> nextBlockSize;
    }
    if (tcpSocket->bytesAvailable() < nextBlockSize) {
        return;
    }

    in >> message;
    QStringList commandList;
    QString fromname;

    enum class COMMAND { NONE, USERLIST, FINDUSER, INVITE, GETFILE};
    COMMAND cmd = COMMAND::NONE;

    if (message=="FRLST")
        cmd = COMMAND::USERLIST;

    else if(message=="_GetFILE_")
        cmd = COMMAND::GETFILE;

    else
    {
        commandList = message.split(" ", QString::SkipEmptyParts);
        if(commandList.at(1)==name)
            return;
        fromname = commandList.at(1);

        QStringRef checkCmd(&message, 0, 5);        //Создание строки из заданного диапозона пришедшего сообщения
        if (checkCmd == "_FIN_")
            cmd = COMMAND::FINDUSER;
        if (checkCmd == "_INV_")
            cmd = COMMAND::INVITE;
    }

    switch (cmd)
    {

    case COMMAND::USERLIST:
    {
        QList <QPair <QString, QString>> FriendKey;
        QList <QPair<QString, QString> > lst;
        ChatListVector chatList;

        in >> FriendKey >> lst >> chatList; ;

        for(int i=0; i<FriendKey.size(); i++)
            pubFriendKey.push_back(qMakePair(FriendKey.at(i).first, FriendKey.at(i).second));
        qDebug() << "Key: " << pubFriendKey;
        for(int i=0; i<lst.size(); i++)
            AddUser_Chat(lst.at(i).first, lst.at(i).second, chatList.at(i).second, i);

        break;
    }

    case COMMAND::FINDUSER:
    {
        QString find_user = commandList.at(1);

        if(find_user=="OKFIN" && gl_fname!=name)
        {
            QString pubKey = commandList.at(3) + " " + commandList.at(4);
            pubFriendKey.push_back(qMakePair(gl_fname, pubKey));
            QList <QPair<QString, QString> > a;
            AddUser_Chat(gl_fname, commandList.at(2), a , -1);
            findcont->~findcontacts();
            on_glass_button_clicked();
        }
        else
            findcont->SetErrorLayout(1);
        //qDebug() << "Поиск:(Ключ друга) " << pubFriendKey;
        break;
    }

    case COMMAND::INVITE:
    {
        QList <QPair<QString, QString> > a;
        QString pubKey = commandList.at(3) + " " + commandList.at(4);
        pubFriendKey.push_back(qMakePair(commandList.at(1), pubKey));
        AddUser_Chat(commandList.at(1), commandList.at(2), a , -2);
        qDebug() << "Инвайт:(Ключ друга) " << pubFriendKey;
        break;
    }

    case COMMAND::GETFILE:
    {
        QByteArray buffer;
        QString filename;
        qint64 fileSize;

        QString dirDownloads = ui->label_6->text();
        QDir(dirDownloads).mkdir(dirDownloads);

        in >> fromname >> filename >> fileSize;

        QThread::sleep(2);
        forever
        {
            if (!nextBlockSize) {

                if (quint32(tcpSocket->bytesAvailable()) < sizeof(quint32)) {
                    break;
                }
                in >> nextBlockSize;
            }
            in >> buffer;
            if (tcpSocket->bytesAvailable() < nextBlockSize) {
                break;
            }
        }

        QFile receiveFile(dirDownloads + filename);
        receiveFile.open(QIODevice::ReadWrite);
        receiveFile.write(buffer);
        receiveFile.close();
        buffer.clear();

        QIcon pic(":/new/prefix1/Resource/sendfiles.png");
        if (!vec.empty())
        {
            for(int i=0; i<vec.size(); i++)
                if(vec.at(i)->data(Qt::DisplayRole)==fromname)
                {
                    QListWidgetItem *item = new QListWidgetItem();
                    item->setData(Qt::UserRole + 1, "FROMF");
                    item->setData(Qt::DisplayRole, filename);
                    item->setData(Qt::ToolTipRole, QString::number((float)fileSize/1024, 'f', 2)  + " KB  " +  QDateTime::currentDateTime().toString("dd.MM.yy hh:mm"));
                    item->setData(Qt::DecorationRole, pic);
                    vec.at(i)->setData(Qt::UserRole + 1, "File: " + filename);
                    vec.at(i)->setData(Qt::ToolTipRole, QDateTime::currentDateTime().toString("dd.MM.yy hh:mm"));

                    if(!this->isVisible() && ui->ChBox_Notif->isChecked())
                    {
                        notice->setPopupText("New message (" + fromname + "):\n" + "File: " + filename);
                        notice->show();
                    }

                    chatvec.at(i)->addItem(item);
                    chatvec.at(i)->scrollToBottom();
                    vec.at(i)->setSelected(true);
                    whisperOnClickUsers(vec.at(i));
                    ui->textBrowser->hide();
                    ui->stackedWidget_2->show();
                    ui->stackedWidget_2->setCurrentIndex(i);

                    break;
                }
        }
        nextBlockSize = 0;

        break;
    }
    default:                                                // Получение обычного текстового сообщения. Звук и добавление в ЧатЛист

        if(QStringRef(&message, 0, 3)=="*To")
        {
            QString myMsg = message.remove(0, 6+fromname.size());
            QStringList lst = myPrivateKey.split(" ", QString::SkipEmptyParts);
            myMsg = rsaCrypt->decodeText(myMsg, lst.at(0).toInt(), lst.at(1).toInt());

            QListWidgetItem *item = new QListWidgetItem();
            item->setData(Qt::UserRole + 1, "TO");
            item->setData(Qt::DisplayRole, myMsg);
            item->setData(Qt::ToolTipRole, QDateTime::currentDateTime().toString("dd.MM.yy hh:mm"));

            chatvec.at(ui->stackedWidget_2->currentIndex())->addItem(item);
            chatvec.at(ui->stackedWidget_2->currentIndex())->scrollToBottom();

        }
        else
        {
            if(ui->ChBox_PSound->isChecked())
                QSound::play(":/new/prefix1/Resource/from.wav");

            fromname.chop(1);
            if (!vec.empty())
            {
                for(int i=0; i<vec.size(); i++)
                    if(vec.at(i)->data(Qt::DisplayRole)==fromname)
                    {
                        QString decodmsg = message.remove(0, 9+fromname.size());
                        qDebug() << "Принял: " << decodmsg;
                        QStringList lst = myPrivateKey.split(" ", QString::SkipEmptyParts);
                        decodmsg = rsaCrypt->decodeText(decodmsg, lst.at(0).toInt(), lst.at(1).toInt());

                        QListWidgetItem *item = new QListWidgetItem();
                        item->setData(Qt::UserRole + 1, "FROM");
                        item->setData(Qt::DisplayRole, decodmsg);
                        item->setData(Qt::ToolTipRole, QDateTime::currentDateTime().toString("dd.MM.yy hh:mm"));

                        vec.at(i)->setData(Qt::UserRole + 1, decodmsg);
                        vec.at(i)->setData(Qt::ToolTipRole, QDateTime::currentDateTime().toString("dd.MM.yy hh:mm"));

                        if(!this->isVisible())
                        {
                            notice->setPopupText("New message (" + fromname + "):\n" + decodmsg);
                            notice->show();
                        }
                        chatvec.at(i)->addItem(item);
                        chatvec.at(i)->scrollToBottom();
                        vec.at(i)->setSelected(true);
                        whisperOnClickUsers(vec.at(i));
                        ui->textBrowser->hide();
                        ui->stackedWidget_2->show();
                        ui->stackedWidget_2->setCurrentIndex(i);

                        break;
                    }
                // Если пользователь не из списка (удален ранее), добавить заного в список.
                // Сделать попозже. Добавить в текущей беседе и вновь в БД на сервере.

                //QList <QPair<QString, QString> > a;
                //AddUser_Chat(fromname, commandList.at(2), a , -1);
                //qDebug() << fromname << message;
            }
        }
    }
}


void Client::whisperOnClickUsers(QListWidgetItem* user)
{
    ui->editText->clear();
    ui->editText->setFocus();
}

void Client::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() && Qt::LeftButton) {
        move(event->globalPos() - m_dragPosition);
        event->accept();
    }
}

void Client::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragPosition = event->globalPos() - frameGeometry().topLeft();
        event->accept();
    }
}

void Client::show_Error(QAbstractSocket::SocketError errorSocket)
{
    switch (errorSocket)
    {
    case QAbstractSocket::RemoteHostClosedError:
        QMessageBox::information(this, tr("Chat Client"),
                                 tr("Disconnected from Server."));
        break;
    case QAbstractSocket::HostNotFoundError:
        QMessageBox::information(this, tr("Chat Client"),
                                 tr("The host was not found.\nPlease check the hostname and port settings."));
        break;
    case QAbstractSocket::ConnectionRefusedError:
        QMessageBox::information(this, tr("Chat Client"),
                                 tr("The connection was refused by the peer.\n"
                                    "Make sure the server is running,\n"
                                    "and check that the host name and port\n"
                                    "settings are correct."));
        break;
    default:
        QMessageBox::information(this, tr("Chat Client"),
                                 tr("The following error occurred: %1.").arg(tcpSocket->errorString()));
    }
}

void Client::send_personal_data()
{
    if(!personDates)
    {
        personDates = true;

        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_5_4);

        QString command = "_USR_";
        QString username = ui->usernameEdit->text();
        out << quint32(0) << QTime::currentTime() << command << username ;

        tcpSocket->write(block);
        reg_window->close();
    }
}

void Client::onDisconnect()
{
    personDates = false;
}

void Client::sendUserCommand(QString command, QString mymsg)
{
    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_4);

    out << quint32(0) << QTime::currentTime() << QString("_UCD_") << command << mymsg;
    tcpSocket->write(msg);
}

void Client::on_userSetting_clicked()
{
    ui->widget_2->show();
}

void Client::on_close_setting_button_clicked()
{
    ui->userList_3->clearFocus();
    ui->userList_3->clearSelection();
    ui->stackedWidget->setCurrentIndex(0);
    ui->widget_2->hide();
}

void Client::whisperOnClick(QListWidgetItem* user)
{
    QString section = user->text();
    if (section == "Profile" || section == "Профиль" || section == "Profil")
        ui->stackedWidget->setCurrentIndex(0);
    else if(section=="General" || section == "Основное" || section == "Principal")
        ui->stackedWidget->setCurrentIndex(1);
    else if (section == "Chat options" || section == "Опции чата" || section == "Options du Chat" )
        ui->stackedWidget->setCurrentIndex(2);
    else
        ui->stackedWidget->setCurrentIndex(3);
}

void Client::on_pushButton_clicked()
{
    showEmoji();
}

void Client::showEmoji()
{
    QPoint p = QCursor::pos();
    frameEmoji->setGeometry(p.x()-250, p.y() -300, 300, 250);
    frameEmoji->show();

}

void Client::setGlass()
{
    QPropertyAnimation* anim = new QPropertyAnimation(ui->glass_button);
    QGraphicsOpacityEffect* grEffect = new QGraphicsOpacityEffect(ui->glass_button);
    ui->glass_button->setGraphicsEffect(grEffect);
    anim->setTargetObject(grEffect);
    anim->setPropertyName("opacity");
    anim->setDuration(1000);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->start();
    ui->glass_button->show();
}

void Client::showFindCont()
{
    QPoint p = QCursor::pos();
    findcont->setGeometry(p.x()-200, p.y()-230, 310, 350);
    findcont->show();
    connect(findcont, SIGNAL(findUsers(QString)), this, SLOT(findtoserv(QString)));

}

void Client::findtoserv(QString name_user)
{
    gl_fname = name_user;
    static bool tmp = true;

    if(name==gl_fname)
    {
        findcont->SetErrorLayout(0);
        return;
    }

    for(int i=0; i<vec.size(); i++)
        if (vec.at(i)->data(Qt::DisplayRole)== gl_fname)
        {
            findcont->SetErrorLayout(2);
            return;
        }

    if(tmp)
    {
        QByteArray msg;
        QDataStream out(&msg, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_5_4);

        out << quint32(0) << QTime::currentTime() << QString("_FND_") << name_user << name;
        tcpSocket->write(msg);
        tmp=false;
    }
    else    // Костылек :)
        tmp=true;
}

void Client::on_newContact_Button_clicked()
{
    choice_window = new ChoiceCreate(this);
    QPoint p = QCursor::pos();
    choice_window->setGeometry(p.x() + 330, p.y() + 70, 343, 220);
    setGlass();
    choice_window->show();
    connect(choice_window, SIGNAL(choice(QString)), this, SLOT(choice_Window(QString)));

}

void Client::choice_Window(QString str)
{
    if(str == "close")
        on_glass_button_clicked();

    else if(str == "newContact")
    {
        findcont = new findcontacts();
        showFindCont();
    }
    else if (str == "newGroup")
    {
        create_group = new CreateGroup(this);
        showCreateGroup();
    }
}

void Client::showCreateGroup()
{
    QPoint p = QCursor::pos();
    create_group->setGeometry(p.x()-240, p.y()-175, 400, 230);
    create_group->show();

    connect(create_group, SIGNAL(GroupSignal(QString, QString, QString, QString)), SLOT(getCreateGroupSig(QString, QString, QString, QString)));
}

void Client::getCreateGroupSig(QString state, QString group_name, QString group_descr, QString path_avatar)
{

    if(state=="Close"){
        on_glass_button_clicked();

    }
    else if(state == "Create")
    {
        selectContacts = new SelectContacts(this, ui->userList);
        QPoint p = QCursor::pos();
        selectContacts->setGeometry(p.x()-245, p.y()-280, 361, 540);
        selectContacts->show();

        groupData.push_back(group_name);
        groupData.push_back(group_descr);
        groupData.push_back(path_avatar);

        connect(selectContacts, SIGNAL(SelectUsersSignal(QStringList, QString)), SLOT(addGroup_toList(QStringList, QString)));
    }
}

void Client::addGroup_toList(QStringList userList, QString state)
{
    if(state == "Cancel")
    {
        if(!groupData.isEmpty())
            groupData.clear();
        on_glass_button_clicked();
    }

    else if(state == "Create")
    {
        QListWidgetItem *item = new QListWidgetItem();
        QListWidget *chatlist = new QListWidget();
        chatlist->setItemDelegate(new ChatListDelegate(chatlist, colorchat));
        ui->stackedWidget_2->addWidget(chatlist);

        QString groupName = groupData.takeAt(0);
        item->setData(Qt::DisplayRole, groupName);
        item->setData(Qt::ToolTipRole, QDateTime::currentDateTime().toString("dd.MM.yy hh:mm"));
        item->setData(Qt::UserRole + 1, groupData.takeAt(0));
        //item->setData(Qt::DecorationRole, groupData.takeAt(2));

        QListWidgetItem *item2 = new QListWidgetItem();
        item2->setData(Qt::UserRole + 1, "TO");
        item2->setData(Qt::DisplayRole, "You have created a new room \"" + groupName + "\"");
        item2->setData(Qt::ToolTipRole, QDateTime::currentDateTime().toString("dd.MM.yy hh:mm"));
        chatlist->addItem(item2);


        vec.push_back(item);
        chatvec.push_back(chatlist);
        ui->userList->addItem(item);
        ui->userList->scrollToBottom();

        ui->textBrowser->hide();
        ui->stackedWidget_2->show();
        ui->stackedWidget_2->setCurrentIndex(chatvec.size()-1);
        ui->userList->clearSelection();
        vec.at(vec.size()-1)->setSelected(true);
        whisperOnClickUsers(vec.at(vec.size()-1));
        on_glass_button_clicked();
    }

}

void Client::on_actionShowHideWindow_triggered()
{
    showHideWindow();
}

void Client::on_actionExit_triggered()
{
    QApplication::exit();
}

void Client::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
    case QSystemTrayIcon::Trigger:
        showHideWindow();
        break;
    case QSystemTrayIcon::Unknown:
        break;
    case QSystemTrayIcon::Context:
        break;
    case QSystemTrayIcon::DoubleClick:
        break;
    case QSystemTrayIcon::MiddleClick:
        break;
    default:
        break;
    }
}

void Client::showHideWindow()
{
    if (this->isVisible()) {
        hide();
        ui->actionShowHideWindow->setText(tr("Restore"));
    }
    else {
        show();
        ui->actionShowHideWindow->setText(tr("Hide"));
    }
}

void Client::closeEvent(QCloseEvent *event)
{
    showHideWindow();
    event->ignore();
}


void Client::on_PB_SelColor_clicked()
{
    QColor color = QColorDialog::getColor(Qt::black, this, "Text Color",  QColorDialog::DontUseNativeDialog);
    colorchat = color.name();

    QPixmap pixmap(16,16);
    QPainter painter;
    painter.begin(&pixmap);
    painter.drawRect(0,0,16,16);
    painter.fillRect(0,0,16,16,QBrush(QColor(color)));
    painter.end();
    ui->imageLabel->setPixmap(pixmap);
    ui->chat_back_lab->setPixmap(pixmap);
}

void Client::on_PB_LoadFileBackground_clicked()
{
    QString files = QFileDialog::getOpenFileName(this, tr("Select Images"), "" , tr("Images (*.jpg *jpeg *.png)"));

    if(QString::compare(files, QString())!=0)
    {
        QImage image;
        bool vol = image.load(files);

        if(vol)
        {
            ui->chat_back_lab->setPixmap(QPixmap::fromImage(image));
            ui->imageLabel->setPixmap(QPixmap::fromImage(image));
        }
        else
        {
            //Error
        }
    }
}

void Client::on_radioButton_2_clicked()
{
    QString files = QFileDialog::getOpenFileName(this, tr("Select Images"), "" , tr("Images (*.jpg *jpeg *.png)"));

    if(QString::compare(files, QString())!=0)
    {
        QImage image;
        bool vol = image.load(files);

        if(vol)
        {
            ui->avatar_label->setPixmap(QPixmap::fromImage(image));
            ui->avatar_label->setFixedSize(120,120);
        }
        else
        {
            //Error
        }
    }
}

void Client::on_radioButton_clicked()
{
    QImage image(":/Avatars/Resource/Avatars/5.jpg");
    ui->avatar_label->setPixmap(QPixmap::fromImage(image));
    ui->avatar_label->setFixedSize(120,120);
}

void Client::on_Download_path_PB_clicked()
{
    QString path = QFileDialog::getExistingDirectory(this, tr("Select Images"), "");

    if(QString::compare(path, QString())!=0)
    {
        download_path = path;
        ui->label_6->setText(path);
    }
    else
        ui->label_6->setText(download_path);
}


void Client::on_pushButton_2_clicked()
{
    if(ui->stackedWidget_2->isHidden())
        return;

    QString filePatch = QFileDialog::getOpenFileName(this,
                                                     QObject::trUtf8("Выбор файла для отправки"), "",
                                                     QObject::trUtf8("(*.*)"));
    if (filePatch.isEmpty())
        return;

    QByteArray  arrBlock;
    qint64 fileSize;
    QDataStream out(&arrBlock, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_4);

    sendFile = new QFile(filePatch);
    sendFile->open(QFile::ReadOnly);

    QFileInfo fi(filePatch);
    QString fileName = fi.fileName();

    fileSize = fi.size();

    QByteArray buffer = sendFile->readAll();
    QString receiver_name = vec.at(ui->userList->currentRow())->data(Qt::DisplayRole).toString();

    out << quint32(0) << QTime::currentTime() << QString("_FILE_") << receiver_name
        << fileName << fileSize << buffer;

    out.device()->seek(0);
    out << quint32(arrBlock.size() - sizeof(quint32));

    tcpSocket->write(arrBlock);

    QIcon pic(":/new/prefix1/Resource/sendfiles.png");
    int pos;
    QListWidgetItem *item = new QListWidgetItem();
    item->setData(Qt::UserRole + 1, "TOF");

    if(fileName.size()>55)
    {
        for(int i=fileName.size()-1; i>=0; i--)
            if(fileName.at(i) == '.')
            {
                pos = i;
                //fileName.remove(28, pos);
                QString newfilename = fileName.left(50)+"...."+fileName.right(fileName.size()-pos);
                fileName = newfilename;
                break;
            }
    }

    item->setData(Qt::DisplayRole, fileName);
    item->setData(Qt::ToolTipRole, QString::number((float)fileSize/1024, 'f', 2)  + " KB  " + QDateTime::currentDateTime().toString("dd.MM.yy hh:mm"));
    item->setData(Qt::DecorationRole, pic);
    chatvec.at(ui->stackedWidget_2->currentIndex())->addItem(item);
    chatvec.at(ui->stackedWidget_2->currentIndex())->scrollToBottom();
}


void Client::on_userList_clicked(const QModelIndex &index)
{
    if (!vec.empty())
    {
        ui->textBrowser->hide();
        ui->stackedWidget_2->show();
        ui->stackedWidget_2->setCurrentIndex(index.row());
    }
}

void Client::on_pushButton_3_clicked()
{
    this->close();
}

void Client::on_pushButton_5_clicked()
{
    aboutdialog = new AboutDialog(this);
    QPoint p = QCursor::pos();
    aboutdialog->setGeometry(p.x() - 550, p.y() +80, 380, 480);
    setGlass();
    aboutdialog->show();
}

void Client::clearHistory()
{
    // Очистить историю.
    // Пока сеансово, добавть подтверждение и запрос в БД на очистку переписки.

    QString message = "Are you sure you want to clear history?";
    conf_message = new ConfirmWindow(this, message);
    conf_message->setGeometry(395, 100, 350, 170);
    setGlass();
    conf_message->show();

    connect(conf_message, SIGNAL(response(QString)), this, SLOT(clearCurHistory(QString)));
}

void Client::clearCurHistory(QString cmd)
{
    if(cmd=="OK")
    {
        chatvec.at(ui->stackedWidget_2->currentIndex())->clear();

        QByteArray msg;
        QDataStream out(&msg, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_5_4);

        QString to = vec.at(ui->userList->currentRow())->data(Qt::DisplayRole).toString();
        out << quint32(0) << QTime::currentTime() << QString("_CLNHISTORY_") << name << to;
        tcpSocket->write(msg);
    }
    on_glass_button_clicked();
    conf_message->~ConfirmWindow();
}

void Client::clearCurUser(QString cmd)
{
    if(cmd=="OK")
    {
        // Если есть контакты в списке, удалить нужного.
        if(!ui->userList->size().isEmpty())
        {
            QByteArray msg;
            QDataStream out(&msg, QIODevice::WriteOnly);
            out.setVersion(QDataStream::Qt_5_4);

            QString delfriend = vec.at(ui->userList->currentRow())->data(Qt::DisplayRole).toString();
            out << quint32(0) << QTime::currentTime() << QString("_DELFRIEND_") << name << delfriend;
            tcpSocket->write(msg);

            //Вытаскивание виджета из Стека Виджетов >_<
            QWidget* widget = ui->stackedWidget_2->widget(ui->userList->currentRow());
            // и удалить
            ui->stackedWidget_2->removeWidget(widget);

            // Очистить вектора
            vec.erase(vec.begin()+ui->userList->currentRow());
            chatvec.erase(chatvec.begin()+ui->userList->currentRow());
            ui->userList->removeItemWidget(ui->userList->takeItem(ui->userList->currentRow()));
            update();
        }
    }

    on_glass_button_clicked();
    conf_message->~ConfirmWindow();

}
void Client::ClearSelect()
{
    // Очистить выделенные сообщения
    chatvec.at(ui->stackedWidget_2->currentIndex())->clearSelection();
}

void Client::showContextMenuForChat(const QPoint &pos)
{
    // Контекстное меню для чата.

    if(ui->stackedWidget_2->count()==0 || ui->stackedWidget->isHidden() || chatvec.at(ui->stackedWidget_2->currentIndex())->count()==0)
        return;

    QPoint newPos = pos;
    newPos.setX(pos.x()+385);
    newPos.setY(pos.y()+30);

    QMenu * menu = new QMenu(this);
    QAction * deleteDevice = new QAction(trUtf8("Clear history"), this);
    QAction * delSelect = NULL;
    menu->addAction(deleteDevice);

    // Если есть выделенные сообщения, добавить в меню возможность скипнуть.
    if(ui->stackedWidget_2->count()!=0)
        if(!chatvec.at(ui->stackedWidget_2->currentIndex())->selectedItems().isEmpty())
        {
            delSelect = new QAction(trUtf8("Clear selection"), this);
            menu->addAction(delSelect);
        }

    menu->popup(mapToGlobal(newPos));
    connect(delSelect, SIGNAL(triggered()), this, SLOT(ClearSelect())); // Обработчик удаления записи
    connect(deleteDevice, SIGNAL(triggered()), this, SLOT(clearHistory()));
}

void Client::showContextMenuForWidget(const QPoint &pos)
{
    // Контекстное меню для списка контактов.

    if(ui->userList->count()==0 || ui->userList->selectedItems().isEmpty())
        return;

    QPoint newPos = pos;
    newPos.setX(pos.x());
    newPos.setY(pos.y()+45);

    QMenu * menu = new QMenu(this);
    QAction * deleteDevice = new QAction(trUtf8("Delete user"), this);
    connect(deleteDevice, SIGNAL(triggered()), this, SLOT(DeleteUser())); // Обработчик удаления записи
    menu->addAction(deleteDevice);
    menu->popup(mapToGlobal(newPos));
}

void Client::on_comboBox_currentIndexChanged(int index)
{
    switch (index) {
    case 0:
    {
        ui->checkBox_3->setText("Launch when system starts");
        ui->label_2->setText("Language: ");
        ui->label_3->setText("Download path: ");
        ui->RB_sendEnter->setText("Send by Enter");
        ui->RB_send_CEnter->setText("Send by Ctrl + Enter");
        ui->ChBox_Notif->setText("Notification");
        ui->ChBox_PSound->setText("Play Sound");
        ui->label_7->setText("Chat background:");
        ui->PB_SelColor->setText("Select color:");
        ui->PB_LoadFileBackground->setText(" Load from file:");
        ui->username_label->setText("Username: ");
        ui->radioButton->setText("Default");
        ui->radioButton_2->setText("Choose");
        ui->groupBox->setTitle("Avatar:");
        ui->userList_3->item(0)->setText("Profile");
        ui->userList_3->item(1)->setText("General");
        ui->userList_3->item(2)->setText("Chat options");
        ui->userList_3->item(3)->setText("Network");
        ui->search_line_edit->setPlaceholderText("Search");
        ui->editText->setPlaceholderText("Write a message..");
        break;
    }
    case 1:
    {
        ui->checkBox_3->setText("Загрузка при старте системы");
        ui->label_2->setText("Язык: ");
        ui->label_3->setText("Загрузочный путь: ");
        ui->RB_sendEnter->setText("Отправка по \"Enter\"");
        ui->RB_send_CEnter->setText("Отправка по \"Ctrl + Enter\"");
        ui->ChBox_Notif->setText("Уведомления");
        ui->ChBox_PSound->setText("Звук");
        ui->label_7->setText("Выбор фона:");
        ui->PB_SelColor->setText("Выбрать из палитры:");
        ui->PB_LoadFileBackground->setText("Загрузить из файла:");
        ui->username_label->setText("Имя пользователя: ");
        ui->radioButton->setText("По умолчанию");
        ui->radioButton_2->setText("Загрузить");
        ui->groupBox->setTitle("Аватар:");
        ui->userList_3->item(0)->setText("Профиль");
        ui->userList_3->item(1)->setText("Основное");
        ui->userList_3->item(2)->setText("Опции чата");
        ui->userList_3->item(3)->setText("Сеть");
        ui->search_line_edit->setPlaceholderText("Поиск");
        ui->editText->setPlaceholderText("Введите сообщение..");
        break;
    }
    case 2:
    {
        ui->checkBox_3->setText("Chargement au lancement");
        ui->label_2->setText("Langue de: ");
        ui->label_3->setText("Le chemin d'amorçage: ");
        ui->RB_sendEnter->setText("Envoyer à \"Entrée\"");
        ui->RB_send_CEnter->setText("Envoyer à \"Ctrl + Entrée\"");
        ui->ChBox_Notif->setText("Notification");
        ui->ChBox_PSound->setText("Le son");
        ui->label_7->setText("Sélection de l'fond");
        ui->PB_SelColor->setText("Choisir de palette:");
        ui->PB_LoadFileBackground->setText("Télécharger un fichier:");
        ui->username_label->setText("Nom d'utilisateur: ");
        ui->radioButton->setText("Par défaut");
        ui->radioButton_2->setText("Download");
        ui->groupBox->setTitle("Image");
        ui->userList_3->item(0)->setText("Profil");
        ui->userList_3->item(1)->setText("Principal");
        ui->userList_3->item(2)->setText("Options du Chat");
        ui->userList_3->item(3)->setText("Networking");
        ui->search_line_edit->setPlaceholderText("Recherche en");
        ui->editText->setPlaceholderText("Inscris ton message..");
        break;
    }
    }
}


void Client::DeleteUser()
{
    // И запрос на сервер в БД, удалить из таблицы.
    // Хз что пока делать на стороне удаляемого, вдруг он будет писать, а его уже нет =(((

    QString message = "Are you sure you want to delete this user?";
    conf_message = new ConfirmWindow(this, message);
    conf_message->setGeometry(395, 100, 350, 170);
    setGlass();
    conf_message->show();

    connect(conf_message, SIGNAL(response(QString)), this, SLOT(clearCurUser(QString)));
}


// Поиск по контакт - листу. Вызывается при изменении текст. поля.
void Client::on_search_line_edit_textChanged(const QString &arg1)
{
    // Если запрос пуст, удаление предыдщуих результатов поиска + скрытие виджета.
    if(arg1.isEmpty()){
        ui->search_list->clear();
        ui->search_list->hide();
        return;
    }


    // Если список контактов не пуст, нахожу вхождение поиск-запроса в имена друзей.
    if(!vec.isEmpty())
    {
        ui->search_list->show();
        ui->search_list->clear();
        for(int i=0; i<vec.size(); i++)
            if(vec.at(i)->data(Qt::DisplayRole).toString().contains(arg1, Qt::CaseInsensitive))
            {
                // Во временный список добавляю элементы, которые удовлетворяют поиску(вхождение подстроки)
                // Переписать: не копировать все элементы, а брать их по ссылке.
                QListWidgetItem *item = new QListWidgetItem();
                item->setData(Qt::DisplayRole, vec.at(i)->data(Qt::DisplayRole).toString());
                item->setData(Qt::ToolTipRole, vec.at(i)->data(Qt::ToolTipRole).toString());
                item->setData(Qt::UserRole + 1, vec.at(i)->data(Qt::UserRole + 1).toString());
                item->setData(Qt::DecorationRole, vec.at(i)->data(Qt::DecorationRole));
                ui->search_list->addItem(item);
            }
    }
}

// Выбор контакта из поискового запроса (списка)
void Client::on_search_list_clicked(const QModelIndex &index)
{
    for(int i=0; i<vec.size(); i++)
    {
        if(vec.at(i)->data(Qt::DisplayRole).toString() == index.data(Qt::DisplayRole).toString())
        {
            // Переключение на выбранного человека/чат. Очищение запроса поиска + скрытие результата.
            ui->textBrowser->hide();
            ui->stackedWidget_2->show();
            ui->stackedWidget_2->setCurrentIndex(i);
            ui->userList->setCurrentRow(i);
            ui->search_line_edit->clear();
            ui->search_line_edit->clearFocus();
            ui->search_list->hide();
            ui->search_list->clear();
            return;
        }
    }
}

Client::~Client()
{
    delete ui;
}


void Client::on_glass_button_clicked()
{
    ui->glass_button->hide();
}
