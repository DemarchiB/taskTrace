# taskTrace

Trabalho de mestrado com título "MONITORAÇÃO E DIAGNÓSTICO DE TAREFAS CÍCLICAS DE TEMPO REAL NO LINUX"

Programa de Pós Graduação em Engenharia Elétrica - UDESC - Joinville

Orientador: Douglas Wildgrube Bertol

## Dependências

Para compilar o supervisor, é necessário ta a lib ncurses instalada:

sudo apt-get install libncurses5-dev libncursesw5-dev

Executar make para compilar o projeto

Executar o script de teste para executar algumas tarefas simulando as tarefas do usuário e o Supervisor monitorando elas.

## Descrição

Ferramenta para monitoração e diagnóstico de tarefas sendo escalonadas pelo SCHED_DEADLINE no linux. A ferramenta permite detectar perdas de deadlines, estouros de runtime, entre outras métricas.

Para ser utilizada em um programa real, é necessário incluir as funções de "TaskTrace/TaskTrace.h"

Para realizar uma simulação, há uma pasta "userTest" com algumas tarefas fakes para testes. Basta executar o make na pasta principal do repositório e executar o script "scripts/test.sh". Esse script irá executar o Supervisor e também as tarefas de teste.

Para executar as tarefas de teste e o Supervisor separadamente, rodar os scripts "supervisorRun.sh" e "userTaskRun.sh".

Todos os scripts precisam de permissão de root.