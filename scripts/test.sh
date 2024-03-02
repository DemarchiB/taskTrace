#!/bin/bash

# recompila o projeto
source compile.sh

# testa
mkdir -p ~/mestrado/testes
rm -rf ~/mestrado/testes
cp -r objects ~/mestrado/testes

cd ~/mestrado/testes
./TaskTrace & ./Supervisor &
