#include "authdialog.h"
#include "ui_authdialog.h"
#include <QtNetwork>

AuthDialog::AuthDialog(QWidget *parent) :
        QDialog(parent),
        ui(new Ui::AuthDialog) {
    ui->setupUi(this);
    this->setFocus();

    doLoginPageRequest();

}

AuthDialog::~AuthDialog() {
    delete ui;
}

void AuthDialog::doLoginPageRequest() {
    // Gets the URI for the login page.
    QNetworkRequest request(QUrl("https://oauthaccountmanager.googleapis.com/v1/authadvice"));

    QJsonObject dataObject;

    dataObject.insert("external_browser", true);
    dataObject.insert("report_user_id", true);
    dataObject.insert("system_version", "13.4");
    dataObject.insert("app_version", "2.16.4");
    dataObject.insert("user_id", QJsonArray());
    dataObject.insert("safari_authentication_session", true);
    dataObject.insert("supported_service", QJsonArray());
    dataObject.insert("request_trigger", "ADD_ACCOUNT");
    dataObject.insert("lib_ver", "3.3");
    dataObject.insert("package_name", "com.google.OnHub");
    dataObject.insert("redirect_uri", "com.google.sso.586698244315-vc96jg3mn4nap78iir799fc2ll3rk18s:/authCallback");
    dataObject.insert("device_name", deviceName);
    dataObject.insert("client_id", "586698244315-vc96jg3mn4nap78iir799fc2ll3rk18s.apps.googleusercontent.com");
    dataObject.insert("mediator_client_id", "936475272427.apps.googleusercontent.com");
    dataObject.insert("device_id", deviceId);
    dataObject.insert("hl", "en-US");
    dataObject.insert("device_challenge_request", deviceChallenge);
    dataObject.insert("client_state", clientState);

    QString data = QJsonDocument(dataObject).toJson();

    qDebug() << data;
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkAccessManager *manager = new QNetworkAccessManager();
    connect(manager, &QNetworkAccessManager::finished, this, &AuthDialog::loginPageRequestComplete);
    manager->post(request, data.toUtf8());



//    manager.deleteLater();
}

void AuthDialog::loginPageRequestComplete(QNetworkReply *reply) {
    QByteArray body = reply->readAll();
    QJsonObject obj = QJsonDocument::fromJson(body).object();

    if (obj.contains("uri")) {

        // save random data from request.
        QString path = "./gacc";
        QDir directory;

        // Create folder
        if (!directory.exists(path))
            directory.mkpath(path);

        QFile deviceInfo(path + "/device.bin");
        deviceInfo.open(QIODevice::WriteOnly);
        deviceInfo.write(deviceId.toUtf8());
        deviceInfo.close();
        launchWebEngine(obj["uri"].toString());
    }
}


void AuthDialog::launchWebEngine(QString authUri) {
    ui->gridLayout->removeWidget(ui->progressBar);
    QWebEngineHttpRequest authRequest;

    // Disguise as Apple product...
    authRequest.setHeader("User-Agent",
                          "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_4) AppleWebKit/537.36 (KHTML, like Gecko)");

    authRequest.setUrl(QUrl(authUri));
    ui->gridLayout->addWidget(view, 0, 0);
    view->load(authRequest);
    QWebEngineCookieStore *store = view->page()->profile()->cookieStore();
    store->loadAllCookies();
    connect(store, &QWebEngineCookieStore::cookieAdded, this, &AuthDialog::cookieAdded);
}

void AuthDialog::cookieAdded(QNetworkCookie cookie) {
    if (cookie.name() == "oauth_code") {
        view->setDisabled(true);

        ui->gridLayout->addWidget(ui->progressBar, 1, 0);

        QString oauthCode = cookie.value();

        QNetworkRequest request;
        QString data = "client_id=936475272427.apps.googleusercontent.com&code=" + oauthCode +
                       "&grant_type=authorization_code&scope=https%3A%2F%2Fwww.google.com%2Faccounts%2FOAuthLogin";
        request.setUrl(QUrl("https://www.googleapis.com/oauth2/v4/token"));
        QNetworkAccessManager *manager = new QNetworkAccessManager();
        connect(manager, &QNetworkAccessManager::finished, this, &AuthDialog::saveRefreshToken);
        manager->post(request, data.toUtf8());


    }
}

void AuthDialog::saveRefreshToken(QNetworkReply *reply) {
    QByteArray body = reply->readAll();
    QJsonObject obj = QJsonDocument::fromJson(body).object();
    if (obj.contains("refresh_token")) {
        // Victory!

        QString refreshTkn = obj["refresh_token"].toString();

        QString path = "./gacc";
        QDir directory;

        if (!directory.exists(path))
            directory.mkpath(path);

        // Perform encryption.
        QString key = QSysInfo::machineUniqueId() +
                      QCryptographicHash::hash(QSysInfo::currentCpuArchitecture().toUtf8(), QCryptographicHash::Sha512);


        QString iv = QUuid::createUuid().toString();


        QByteArray keyHash = QCryptographicHash::hash(key.toLocal8Bit(), QCryptographicHash::Sha256);
        QByteArray ivHash = QCryptographicHash::hash(iv.toLocal8Bit(), QCryptographicHash::Md5);
        QByteArray enc = QAESEncryption::Crypt(QAESEncryption::AES_256, QAESEncryption::CBC,
                                               refreshTkn.toLocal8Bit(), keyHash, ivHash);
        QJsonObject file;

        file.insert("IV", QTextCodec::codecForMib(106)->toUnicode(ivHash.toBase64()));
        file.insert("dat", QTextCodec::codecForMib(106)->toUnicode(enc.toBase64()));

        // Create the new file.
        QFile credentials("./gacc/auth.bin");
        credentials.open(QIODevice::WriteOnly);
        credentials.write(QJsonDocument(file).toJson());
        credentials.close();

        emit authComplete(refreshTkn);
        close();
    }
}