#! bash
cd "$(dirname "$0")" || exit

openssl req -x509 -newkey ec:<(openssl ecparam -name prime256v1) -nodes \
-keyout server.key -out server.crt -days 365 \
-subj "/C=CN/ST=Guangdong/L=Yunfu/O=TestServer/CN=test.local"