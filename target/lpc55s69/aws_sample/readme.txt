    TOPPERS/SafeG-M
    LPC55S6X AWSサンプル ユーザマニュアル

    対応バージョン: Release x.x.x
    最終更新: 2020年xx月xx日

○概要
本サンプルはSecure側でASP3のsample1を動作させ，Non-secureでFreeRTOSのAWS Shadow
サンプルを動作させる．

○事前に必要な設定
●Nonsecure/amazon-freertos/demos/include/aws_clientcredential.h
    AWSのendpointのアドレスを設定する．
    AWSのthing nameを設定する．
    アクセスポイントのSSIDとパスワードを設定する．

●Nonsecure/amazon-freertos/demos/include/aws_clientcredential_keys.h
    AWSの認証に使用する証明書を設定する．
    このファイルはFreeRTOSのCertificateConfigurator.htmlを利用して生成できる．
    このファイルは以下のリポジトリ等で入手できる．
    https://github.com/aws/amazon-freertos/tree/master/tools/certificate_configuration

○デバッグ方法
Secure側のプログラムをデバッグする際に読み込む．
    デバッガの設定からロードするよう指示できる．
起動するとASP3のサンプルが起動する．
ASP3のタスクが全て終了するか待ち状態になると、Non-secureでAWSサンプルが起動する．
ブラウザでAWS Shadowを見ると、状態が更新されていることが確認できる．
