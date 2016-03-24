#include "selectcontacts.h"
#include "ui_selectcontacts.h"
#include <QDebug>

SelectContacts::SelectContacts(QWidget *parent, QListWidget *list) :
    QFrame(parent),
    ui(new Ui::SelectContacts)
{
    ui->setupUi(this);
    this->setWindowFlags(Qt::Popup | Qt::Window);
    setWindowOpacity(0);
    show();

    QPropertyAnimation* animation = new QPropertyAnimation(this, "windowOpacity");
    animation->setDuration(500);
    animation->setStartValue(0);
    animation->setEndValue(1);
    animation->start();

    ui->userList->setItemDelegate(new UserListDelegate(ui->userList));

    for(int i=0; i<list->count(); i++)
    {
        QListWidgetItem *item = new QListWidgetItem();
        item->setData(Qt::DisplayRole, list->item(i)->data(Qt::DisplayRole).toString());
        item->setData(Qt::ToolTipRole, list->item(i)->data(Qt::ToolTipRole).toString());
        item->setData(Qt::UserRole + 1, list->item(i)->data(Qt::UserRole + 1).toString());
        item->setData(Qt::DecorationRole, list->item(i)->data(Qt::DecorationRole));
        ui->userList->addItem(item);
    }

    ui->count_member->setText("Add members: 0/" + QString::number(list->count()));

    //userList = new QListWidget;
    //userList = list;
    //userList->setItemDelegate(new ChatListDelegate(chatlist, colorchat));
}

SelectContacts::~SelectContacts()
{
    //delete ui;
}

void SelectContacts::on_crGroup_button_clicked()
{
    QStringList userList;
    if(!ui->userList->selectedItems().isEmpty())
    {
        for(int i=0; i<ui->userList->count(); i++)
        {
            if(ui->userList->item(i)->isSelected())
                userList.push_back(ui->userList->item(i)->data(Qt::DisplayRole).toString());
        }

        emit SelectUsersSignal(userList, "Create");
        this->close();
    }
}

void SelectContacts::on_cancel_button_clicked()
{
    QStringList tmp;
    emit SelectUsersSignal(tmp, "Cancel");
    this->close();
}