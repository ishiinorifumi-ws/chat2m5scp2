# chat2m5scp2
ラクネコという来客管理SaaSで、Google Chat に通知があった際に、それを拾って M5StickC Plus2 でブザーを鳴らす、というスケッチです。

全体像としては、以下の流れ

　通知があったことをSQSにセットする部分：　ラクネコ　→　Google Chat スペースへ通知　→　Gasがポーリングして通知をチェックし、AWS API Gateway を通じてLambda を叩き、状態変更のSQSをセットしておく

　M5StickCからAPI Gateway をポーリングして Lawmbdaを通じてSQSにある状態を渡す　→　新しい通知があったら M5のブザーを鳴らして青く光らせる

