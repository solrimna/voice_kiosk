#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QNetworkRequest>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>
#include <QFile>            // 파일 전송을 위해 추가
#include <QHttpMultiPart>   // 파일 전송을 위해 추가
#include <QHttpPart>        // 파일 전송을 위해 추가
#include <QFileInfo>
#include <QTextCursor>      // 자동 스크롤용 추가

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , networkManager(new QNetworkAccessManager(this))
    , uploadManager(new QNetworkAccessManager(this))    // 업로드 전용으로 별도로 만듬
    , captureSession(new QMediaCaptureSession(this))
    , audioInput(new QAudioInput(this))
    , mediaRecorder(new QMediaRecorder(this))
    , mediaPlayer(new QMediaPlayer(this))       // tts 재생용
    , audioOutput(new QAudioOutput(this))       // tts 재생용
    , azureRecognizer(nullptr)
    , isRecognizing(false)
    , recognizedText("")
{
    ui->setupUi(this);

    // 버튼 클릭 이벤트 연결
    connect(ui->pushButton, &QPushButton::clicked,
            this, &MainWindow::onStartButtonClicked);

    // 네트워크 응답 이벤트 연결(메뉴 조회용)
    connect(networkManager, &QNetworkAccessManager::finished,
            this, &MainWindow::onNetworkReply);

    // 녹음 버튼 연결
    connect(ui->btnStartRecord, &QPushButton::clicked,
            this, &MainWindow::onStartRecord);
    connect(ui->btnStopRecord, &QPushButton::clicked,
            this, &MainWindow::onStopRecord);

    // Azure Speech 테스트 버튼 연결
    connect(ui->btnTestAzure, &QPushButton::clicked,
            this, &MainWindow::onTestAzureSpeech);

    // 오디오 녹음 설정
    captureSession->setAudioInput(audioInput);
    captureSession->setRecorder(mediaRecorder);

    // 오디오 출력 설정
    mediaPlayer->setAudioOutput(audioOutput);
    audioOutput->setVolume(1.0);  // 볼륨 최대

    // 초기 버튼 상태
    ui->btnStopRecord->setEnabled(false);

    // 초기 메시지 설정 -> placeholderText 가 한 줄만 지원해서 생성자에서 처리함
    ui->textEditRealtime->setAlignment(Qt::AlignCenter);
    ui->textEditRealtime->setPlainText(
        "🎤 음성으로 주문하세요\n\n"
        "아래 '음성으로 주문하기' 버튼을 누르고\n"
        "주문하실 내용을 말씀해주세요\n\n"
        "예시: 아메리카노 주세요"
        );

    // 회색으로 표시 (힌트처럼)
    QPalette palette = ui->textEditRealtime->palette();
    palette.setColor(QPalette::Text, Qt::gray);
    ui->textEditRealtime->setPalette(palette);

    // 초기 상태
    ui->labelStatus->setText("상태: 대기 중");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onStartButtonClicked()
{
    QUrl url("http://127.0.0.1:8000/api/menu/items/");
    QNetworkRequest request(url);

    networkManager->get(request);

    QMessageBox::information(this, "알림", "서버 요청 TEST...");
}

// 네트워크 응답 처리
void MainWindow::onNetworkReply(QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        // 성공적으로 응답 받음
        QByteArray response = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(response);
        //QJsonObject obj = doc.object();
        QJsonArray array = doc.array();  // 배열로 받음!

        if (!array.isEmpty()) {
            QJsonObject menu = array[0].toObject();
            QString name = menu["name"].toString();
            int price = menu["price"].toInt();

            QString message = QString("메뉴: %1\n가격: %2원").arg(name).arg(price);
            QMessageBox::information(this, "메뉴 정보", message);
        }
    } else {
        // 에러 발생
        QMessageBox::critical(this, "에러", "서버 통신 실패: " + reply->errorString());
    }

    reply->deleteLater();
}
// 녹음 시작 버튼
void MainWindow::onStartRecord()
{
    using namespace Microsoft::CognitiveServices::Speech;
    using namespace Microsoft::CognitiveServices::Speech::Audio;

    qDebug() << "=== Azure 실시간 인식 시작 ===";

    // 이미 인식 중이면 무시
    if (isRecognizing) {
        qDebug() << "이미 인식 중입니다";
        return;
    }

    // 환경변수에서 API 키 가져오기
    QString apiKey = qgetenv("AZURE_SPEECH_KEY");
    QString region = qgetenv("AZURE_SPEECH_REGION");

    if (apiKey.isEmpty() || region.isEmpty()) {
        QMessageBox::critical(this, "환경변수 오류",
                              "Azure Speech API 키가 설정되지 않았습니다!\n\n"
                              "Qt Creator → Projects → Run → Environment에서\n"
                              "AZURE_SPEECH_KEY와 AZURE_SPEECH_REGION을 설정하세요.");
        return;
    }

    try {
        // Speech Config 생성
        auto config = SpeechConfig::FromSubscription(
            apiKey.toStdString(),
            region.toStdString()
            );
        config->SetSpeechRecognitionLanguage("ko-KR");

        // 오디오 설정 (기본 마이크)
        auto audioConfig = AudioConfig::FromDefaultMicrophoneInput();

        // Speech Recognizer 생성
        azureRecognizer = SpeechRecognizer::FromConfig(config, audioConfig);

        qDebug() << "Speech Recognizer 생성 완료";

        // ─── 이벤트 핸들러 연결 ─────────────────────────────────

        // 중간 결과 (말하는 동안 실시간)
        azureRecognizer->Recognizing.Connect([this](const SpeechRecognitionEventArgs& e) {
            QString text = QString::fromStdString(e.Result->Text);
            qDebug() << "중간 결과:" << text;

            // Qt 메인 스레드에서 실행
            QMetaObject::invokeMethod(this, [this, text]() {
                onRecognizingText(text);
            }, Qt::QueuedConnection);
        });

        // 최종 결과 (문장 완료)
        azureRecognizer->Recognized.Connect([this](const SpeechRecognitionEventArgs& e) {
            if (e.Result->Reason == ResultReason::RecognizedSpeech) {
                QString text = QString::fromStdString(e.Result->Text);
                qDebug() << "최종 결과:" << text;

                // Qt 메인 스레드에서 실행
                QMetaObject::invokeMethod(this, [this, text]() {
                    onRecognizedText(text);
                }, Qt::QueuedConnection);
            }
        });

        // 에러 처리
        azureRecognizer->Canceled.Connect([this](const SpeechRecognitionCanceledEventArgs& e) {
            QString error = QString::fromStdString(e.ErrorDetails);
            qDebug() << "인식 취소/오류:" << error;

            // Qt 메인 스레드에서 실행
            QMetaObject::invokeMethod(this, [this, error]() {
                onRecognitionError(error);
            }, Qt::QueuedConnection);
        });

        // ─── 연속 인식 시작 ─────────────────────────────────────

        azureRecognizer->StartContinuousRecognitionAsync().get();

        isRecognizing = true;
        recognizedText.clear();

        // UI 업데이트
        ui->textEditRealtime->clear();
        QPalette palette = ui->textEditRealtime->palette();
        palette.setColor(QPalette::Text, Qt::black);
        ui->textEditRealtime->setPalette(palette);

        ui->labelStatus->setText("상태: 🎤 듣는 중...");
        ui->labelStatus->setStyleSheet("color: red; font-weight: bold;");

        ui->btnStartRecord->setEnabled(false);
        ui->btnStopRecord->setEnabled(true);

        qDebug() << "연속 인식 시작됨";

    } catch (const std::exception& e) {
        QString error = QString::fromStdString(e.what());
        qDebug() << "예외 발생:" << error;

        QMessageBox::critical(this, "오류",
                              QString("음성 인식 시작 실패:\n\n%1").arg(error));

        isRecognizing = false;
    }
}

// 녹음 중지
void MainWindow::onStopRecord()
{
    qDebug() << "=== Azure 실시간 인식 중지 ===";

    if (!isRecognizing || !azureRecognizer) {
        qDebug() << "인식 중이 아닙니다";
        return;
    }

    try {
        // 연속 인식 중지
        azureRecognizer->StopContinuousRecognitionAsync().get();

        qDebug() << "연속 인식 중지됨";

        isRecognizing = false;

        // UI 업데이트
        ui->labelStatus->setText("상태: 🤔 처리 중...");
        ui->labelStatus->setStyleSheet("color: orange; font-weight: bold;");

        ui->btnStartRecord->setEnabled(true);
        ui->btnStopRecord->setEnabled(false);

        // 최종 텍스트가 있으면 Django로 전송
        if (!recognizedText.trimmed().isEmpty()) {
            qDebug() << "최종 텍스트:" << recognizedText;
            sendTextToDjango(recognizedText.trimmed());
        } else {
            QMessageBox::warning(this, "알림",
                                 "인식된 음성이 없습니다.\n다시 시도해주세요.");

            ui->labelStatus->setText("상태: 대기 중");
            ui->labelStatus->setStyleSheet("");
        }

        // Recognizer 해제
        azureRecognizer = nullptr;

    } catch (const std::exception& e) {
        QString error = QString::fromStdString(e.what());
        qDebug() << "중지 중 예외:" << error;

        QMessageBox::critical(this, "오류",
                              QString("음성 인식 중지 실패:\n\n%1").arg(error));

        isRecognizing = false;
        azureRecognizer = nullptr;

        ui->btnStartRecord->setEnabled(true);
        ui->btnStopRecord->setEnabled(false);
    }
}

// 파일 업로드 함수
void MainWindow::uploadVoiceFile(const QString &filePath)
{
    QFile *file = new QFile(filePath);

    if (!file->open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, "에러", "파일을 열 수 없습니다: " + filePath);
        delete file;
        return;
    }

    // Multipart 생성
    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    // 파일 파트 생성
    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("audio/wav"));
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant("form-data; name=\"voice_file\"; filename=\"" + QFileInfo(filePath).fileName() + "\""));

    filePart.setBodyDevice(file);
    file->setParent(multiPart);  // multiPart 삭제 시 file도 같이 삭제

    multiPart->append(filePart);

    // POST 요청
    QUrl url("http://127.0.0.1:8000/api/voice/upload/");
    QNetworkRequest request(url);

    QNetworkReply *reply = uploadManager->post(request, multiPart);
    multiPart->setParent(reply);  // reply 삭제 시 multiPart도 같이 삭제

    // 업로드 완료 시 처리 - 람다 함수로 reply를 캡처해서 전달
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onUploadFinished(reply);
    });
}

// 업로드 응답 처리
void MainWindow::onUploadFinished(QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response = reply->readAll();

        qDebug() << "Django 응답:" << response;

        QJsonDocument doc = QJsonDocument::fromJson(response);
        QJsonObject obj = doc.object();

        qDebug() << "파싱된 JSON:" << obj;

        QString userMessage = obj["user_message"].toString();  // STT 결과
        QString aiResponse = obj["ai_response"].toString();    // ai 응답
        QString ttsUrl = obj["tts_url"].toString();             // TTS URL

        qDebug() << "사용자:" << userMessage;
        qDebug() << "AI:" << aiResponse;
        qDebug() << "TTS URL:" << ttsUrl;

        // 대화 형식으로 표시
        QString resultMsg = QString("👤 고객님:\n%1\n\n🤖 AI 도우미:\n%2\n\n🔊 음성으로 재생 중...")
                                .arg(userMessage)
                                .arg(aiResponse);

        QMessageBox::information(this, "AI 키오스크", resultMsg);

        // 파일 이름 & 사이즈는 확인했으므로 주석 처리 =======================
        //
        // QString fileName = obj["file_name"].toString();
        // int fileSize = obj["file_size"].toInt();

        // 디버그: 개별 값 출력
        // qDebug() << "message:" << message;
        // qDebug() << "fileName:" << fileName;
        // qDebug() << "fileSize:" << fileSize;
        // qDebug() << "STT 결과:" << user_message;
        // QString resultMsg = QString("%1\n파일명: %2\n크기: %3 bytes\n\n 음성인식 결과:\n「%4」")
        //                         .arg(message,
        //                              fileName,
        //                              QString::number(fileSize),
        //                              user_message.isEmpty() ? "인식 실패" : transcription);
        // ================================================================

        // TTS 음성 재생! (새로 추가!)
        if (!ttsUrl.isEmpty()) {
            QString fullUrl = "http://127.0.0.1:8000" + ttsUrl;
            qDebug() << "TTS 재생:" << fullUrl;

            mediaPlayer->setSource(QUrl(fullUrl));
            mediaPlayer->play();
        }

    } else {
        QMessageBox::critical(this, "오류", reply->errorString());
    }

    reply->deleteLater();
}

void MainWindow::onTestAzureSpeech()
{
    using namespace Microsoft::CognitiveServices::Speech;
    using namespace Microsoft::CognitiveServices::Speech::Audio;

    qDebug() << "=== Azure Speech 테스트 시작 ===";

    // 환경변수에서 API 키 가져오기
    QString apiKey = qgetenv("AZURE_SPEECH_KEY");
    QString region = qgetenv("AZURE_SPEECH_REGION");

    // 환경변수 확인
    if (apiKey.isEmpty() || region.isEmpty()) {
        QMessageBox::critical(this, "환경변수 오류",
                              "환경변수가 설정되지 않았습니다!\n\n"
                              "필요한 환경변수:\n"
                              "- AZURE_SPEECH_KEY\n"
                              "- AZURE_SPEECH_REGION\n\n"
                              "Qt Creator의 Projects → Run → Environment에서 설정하세요.");
        return;
    }

    qDebug() << "API Key 확인: (길이:" << apiKey.length() << ")";
    qDebug() << "Region:" << region;

    try {
        // Speech Config 생성
        auto config = SpeechConfig::FromSubscription(
            apiKey.toStdString(),
            region.toStdString()
            );

        // 한국어 설정
        config->SetSpeechRecognitionLanguage("ko-KR");

        qDebug() << "Speech Config 생성 완료";

        // 오디오 설정 (기본 마이크)
        auto audioConfig = AudioConfig::FromDefaultMicrophoneInput();

        qDebug() << "Audio Config 생성 완료";

        // Speech Recognizer 생성
        auto recognizer = SpeechRecognizer::FromConfig(config, audioConfig);

        qDebug() << "Speech Recognizer 생성 완료";

        // 사용자에게 안내
        QMessageBox::information(this, "준비 완료",
                                 "🎤 마이크가 준비되었습니다!\n\n"
                                 "OK 버튼을 누르면 5초 안에 말씀하세요.\n\n"
                                 "예시:\n"
                                 "- 안녕하세요\n"
                                 "- 아메리카노 주세요\n"
                                 "- 테스트입니다");

        qDebug() << "음성 인식 시작...";

        // 음성 인식 (한 번, 5초 타임아웃, get() 결과가 올 때까지 대기)
        auto result = recognizer->RecognizeOnceAsync().get();

        qDebug() << "음성 인식 완료";

        // 결과 처리
        if (result->Reason == ResultReason::RecognizedSpeech) {
            QString text = QString::fromStdString(result->Text);
            qDebug() << "✅ 인식 성공:" << text;

            QMessageBox::information(this, "인식 성공!",
                                     QString("✅ 인식된 텍스트:\n\n「%1」\n\n"
                                             "Azure Speech 연동 성공!").arg(text));
        }
        // 인식 실패(소음 또는 너무 작은 소리 일 때)
        else if (result->Reason == ResultReason::NoMatch) {
            qDebug() << "❌ 음성 인식 실패";

            QMessageBox::warning(this, "인식 실패",
                                 "음성을 인식하지 못했습니다.\n\n"
                                 "다시 시도해주세요.\n"
                                 "마이크가 제대로 연결되어 있는지 확인하세요.");
        }
        // 오류 (API키 틀림, 네트워크 문제 etc...)
        else if (result->Reason == ResultReason::Canceled) {
            auto cancellation = CancellationDetails::FromResult(result);
            QString errorDetails = QString::fromStdString(cancellation->ErrorDetails);

            qDebug() << "❌ 취소됨:" << errorDetails;

            QMessageBox::critical(this, "오류 발생",
                                  QString("음성 인식이 취소되었습니다.\n\n"
                                          "오류: %1\n\n"
                                          "API 키와 지역(Region)을 확인하세요.").arg(errorDetails));
        }

    }
    catch (const std::exception& e) {
        QString error = QString::fromStdString(e.what());
        qDebug() << "❌ 예외 발생:" << error;

        QMessageBox::critical(this, "예외 발생",
                              QString("예외가 발생했습니다:\n\n%1").arg(error));
    }

    qDebug() << "=== Azure Speech 테스트 종료 ===";
}

// 중간 결과 (회색으로 표시)
void MainWindow::onRecognizingText(const QString& text)
{
    QString displayText = recognizedText + text;

    ui->textEditRealtime->setPlainText(displayText);

    // 중간 결과는 회색
    QPalette palette = ui->textEditRealtime->palette();
    palette.setColor(QPalette::Text, Qt::gray);
    ui->textEditRealtime->setPalette(palette);

    // 자동 스크롤
    ui->textEditRealtime->moveCursor(QTextCursor::End);
}

// 최종 결과 누적
void MainWindow::onRecognizedText(const QString& text)
{
    recognizedText += text + " ";

    ui->textEditRealtime->setPlainText(recognizedText);

    // 최종 결과는 검정색
    QPalette palette = ui->textEditRealtime->palette();
    palette.setColor(QPalette::Text, Qt::black);
    ui->textEditRealtime->setPalette(palette);

    // 자동 스크롤
    ui->textEditRealtime->moveCursor(QTextCursor::End);

    qDebug() << "누적 텍스트:" << recognizedText;
}

// 에러시 호출
void MainWindow::onRecognitionError(const QString& error)
{
    qDebug() << "인식 오류:" << error;

    isRecognizing = false;
    azureRecognizer = nullptr;

    // UI 복구
    ui->labelStatus->setText("상태: 오류 발생");
    ui->labelStatus->setStyleSheet("color: red;");

    ui->btnStartRecord->setEnabled(true);
    ui->btnStopRecord->setEnabled(false);

    QMessageBox::critical(this, "음성 인식 오류",
                          QString("음성 인식 중 오류가 발생했습니다:\n\n%1\n\n"
                                  "API 키와 네트워크 연결을 확인하세요.").arg(error));
}

void MainWindow::sendTextToDjango(const QString& text)
{
    qDebug() << "Django로 텍스트 전송:" << text;

    // Django API 엔드포인트
    QUrl url("http://127.0.0.1:8000/api/voice/process_text/");

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // JSON 데이터 생성
    QJsonObject json;
    json["text"] = text;
    QJsonDocument doc(json);
    QByteArray data = doc.toJson();

    // POST 요청
    QNetworkReply *reply = uploadManager->post(request, data);

    // 응답 처리
    connect(reply, &QNetworkReply::finished, [this, reply]() {
        onUploadFinished(reply);
    });

    qDebug() << "전송 완료, 응답 대기 중...";
}
