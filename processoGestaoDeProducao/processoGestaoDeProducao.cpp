#define WIN32_LEAN_AND_MEAN
#include <windows.h>    // MAX_PATH, CreateProcess, GetModuleFileName
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <iostream>
#include <filesystem>
#include <conio.h>		// _getch
#include <sstream>

#define NOME_MAISLOT L"\\\\.\\mailslot\\comunicacaoPCP"

typedef unsigned* CAST_LPDWORD;

struct mensagem {
	int codigo;
	char conteudo[256];
};

struct mensagemPCP {
	unsigned int TIPO = 2;         // Sempre "2"
	unsigned int NSEQ;             // Número sequencial da mensagem
	std::string HORA;              // Hora prevista para início da OP
	unsigned int DURACAO;          // Tempo previsto para execução da OP (minutos)
	char OP[9];                    // Número da Ordem de Produção (OP) em execução considera o '\0' no final
};
struct DadosRecepcao {
    HANDLE mailslotHandle;
    HANDLE eventoDeEncerramento;
};

HANDLE criarMailslot(HANDLE eventoCriacaoMailSlot) {

	HANDLE mailslotHandle = CreateMailslotW(
		NOME_MAISLOT,
		0,									// Tamanho máximo da mensagem (0 significa tamanho ilimitado)
		MAILSLOT_WAIT_FOREVER,				// Tempo de espera para leitura (infinito)
		NULL								// Segurança padrão
	);

	if (mailslotHandle == INVALID_HANDLE_VALUE) {
		std::cout << "[Processo de Gestão de Produção] Erro ao criar o mailslot no processo de gestão de produção. Código de saída: " << GetLastError() << std::endl;
		return INVALID_HANDLE_VALUE;
	} else {
		std::cout << "[Processo de Gestão de Produção] Mailslot criado com sucesso." << std::endl;

		if (!SetEvent(eventoCriacaoMailSlot)) {
			std::cout << "[Processo de Gestão de Produção] Erro ao sinalizar o evento de criação do mailslot. Código de saída: " << GetLastError() << std::endl;
			return INVALID_HANDLE_VALUE;
		}
		else {
			std::cout << "[Processo de Gestão de Produção] Evento de criação do mailslot sinalizado com sucesso." << std::endl;
		}

		return mailslotHandle;
	}

}

bool IsAInterpretavelMessage(const std::string& texto, mensagemPCP& mensagem){
	if (texto.size() != 31) return false;
	
	// Mensagens sao separadas por "/"
	if (texto[1] != '/' || texto[7] != '/' || texto[16] != '/' || texto[22] != '/') return false;
	 try {
        if (texto[0] != '2') return false;

        mensagem.TIPO = 2;
        mensagem.NSEQ = std::stoul(texto.substr(2, 5));
        mensagem.HORA = texto.substr(8, 8);
        mensagem.DURACAO = std::stoul(texto.substr(17, 5));
        std::string op = texto.substr(23, 8);
        strcpy_s(mensagem.OP, op.c_str());

 	} catch (const std::exception&) {
        return false;
    }

    return true;    
}

unsigned __stdcall receberMensagens(void* param) {

	
    DadosRecepcao* dados = static_cast<DadosRecepcao*>(param);

    HANDLE mailslotHandle = dados->mailslotHandle;

    HANDLE eventoDeEncerramento = dados->eventoDeEncerramento;
      

	if (mailslotHandle == INVALID_HANDLE_VALUE) {
		std::cout << "[Processo de Gestão de Produção] Handle do mailslot inválido. Código de saída: " << GetLastError() << std::endl;
		return 1;
	}

	DWORD tamanhoDaMensagem;
	DWORD quantidadeDeMensagens;
	DWORD bytesLidos;

	//mensagemPCP Mensagem{};
	char mensagemRecebida[32] = {};

	
	while (true) {
      
		
		DWORD estado = WaitForSingleObject(eventoDeEncerramento, 0);

		if (estado == WAIT_OBJECT_0) {
			break;
		}

		if (estado == WAIT_FAILED) {
			std::cout << "Erro ao verificar encerramento: "
					<< GetLastError() << std::endl;
			return 1;
		}
		bool existemMensagens = GetMailslotInfo(
			mailslotHandle,
			NULL,
			&tamanhoDaMensagem,
			&quantidadeDeMensagens,
			NULL						
		);

		if (!existemMensagens) {
			std::cout << "[Processo de Gestão de Produção] Erro ao obter informações do mailslot. Código de saída: " << GetLastError() << std::endl;
			return 1;
		}

		if (quantidadeDeMensagens > 0) {
			if (tamanhoDaMensagem >= sizeof(mensagemRecebida)) {
				std::cout << "Mensagem maior que o buffer de recepcao." << std::endl;
				return 1;
			}
			bool leituraBemSucedida = ReadFile(
				mailslotHandle,
				mensagemRecebida,
				sizeof(mensagemRecebida) - 1,
				&bytesLidos,
				NULL
			);

			if (!leituraBemSucedida) {
				std::cout << "[Processo de Gestão de Produção] Erro ao ler mensagem do mailslot. Código de saída: " << GetLastError() << std::endl;
				return 1;

			} else {
				if (bytesLidos != 31) {
					std::cout << "[Gestao] Tamanho de mensagem invalido: " << bytesLidos << std::endl;
					continue;
				}
				mensagemRecebida[bytesLidos] = '\0';
				std::cout << "[Processo de Gestão de Produção] Mensagem recebida: "<< mensagemRecebida << std::endl;
				 mensagemPCP mensagem{};

				if (!IsAInterpretavelMessage(mensagemRecebida, mensagem)) {
					std::cout << "[Gestao] Mensagem PCP invalida."<< std::endl;
					continue;
				}
				std::cout << "[Gestao] TIPO: " << mensagem.TIPO
						<< " | NSEQ: " << mensagem.NSEQ
						<< " | HORA: " << mensagem.HORA
						<< " | DURACAO: " << mensagem.DURACAO
						<< " | OP: " << mensagem.OP
						<< std::endl;
			}
		}
		DWORD espera = WaitForSingleObject( eventoDeEncerramento, 50);

		if (espera == WAIT_OBJECT_0) break;
		if (espera == WAIT_FAILED) {
			std::cout << "Erro ao aguardar evento de encerramento: "<< GetLastError() << std::endl;
			return 1;
		}
	}
	return 0;
}

unsigned __stdcall monitorarEncerramento(void* param) {

	HANDLE eventoDeEncerramento = static_cast<HANDLE>(param);

	if (eventoDeEncerramento == nullptr) {
		std::cout << "[Processo de Gestão de Produção] Handle do evento inválido. Código de saída: " << GetLastError() << std::endl;
		return 1;

	} else {

		DWORD result = WaitForSingleObject(eventoDeEncerramento, INFINITE);

		if (result == WAIT_OBJECT_0) {
			std::cout << "[Processo de Gestão de Produção] Evento de encerramento sinalizado. Encerrando o processo de gestão de produção." << std::endl;
			return 0;

		} else if (result == WAIT_FAILED) {
			std::cout << "[Processo de Gestão de Produção] Erro ao aguardar o evento de encerramento. Código: " << GetLastError() << std::endl;
			return 1;
		}
	}
	return 1;
}

void limparConsole() {
	// Limpa a tela do console
	system("cls");
}

unsigned __stdcall monitorarComandoDeLimpezaDoConsole(void* param) {
	HANDLE eventoDeLimparJanelaDaTarefaDeExibicaoDosDadosDeProcesso = static_cast<HANDLE>(param);

	while (true) {
		DWORD resultado = WaitForSingleObject(
			eventoDeLimparJanelaDaTarefaDeExibicaoDosDadosDeProcesso,
			INFINITE);

		if (resultado == WAIT_OBJECT_0) {
			limparConsole();
		}
		else {
			break;
		}
	}

	return 0;
}

int main() {

	SetConsoleOutputCP(CP_UTF8); // Configura a saída do console para UTF-8
	std::cout << "[Processo de Gestão de Produção] Este é o processo de gestão de produção.\n";

	// Seção de abertura de eventos
	HANDLE eventoDeEncerramento = OpenEventW(SYNCHRONIZE,
											 FALSE, 
											 L"eventoDeEncerramentoDosProcessosFilhos");

	if (eventoDeEncerramento == nullptr) {
		std::cout << "[Processo de Gestão de Produção] Erro ao abrir o evento de encerramento. Código de saída: " << GetLastError() << std::endl;
		return 1;
	}

	HANDLE eventoDeLimparJanelaDaTarefaDeGestaoDaProducao = OpenEventW(SYNCHRONIZE,
		FALSE,
		L"eventoDeLimparJanelaDaTarefaDeGestaoDaProducao");

	if (eventoDeLimparJanelaDaTarefaDeGestaoDaProducao == nullptr) {
		std::cout << "Erro ao abrir o evento de limpeza do console de Exibição. Código de saída: " << GetLastError() << std::endl;
		return 1;
	}

	// Seção de criação de eventos
	HANDLE eventoCriacaoMailSlot = CreateEventW(nullptr,
								   true,
								   false,
								   L"eventoCriacaoMailSlot");

	if (eventoCriacaoMailSlot == nullptr) {
		std::cout << "[Processo de Gestão de Produção] Erro ao abrir o evento de criação do mailslot. Código de saída: " << GetLastError() << std::endl;
		return 1;
	}

	HANDLE mailslotHandle = criarMailslot(eventoCriacaoMailSlot);

	if (mailslotHandle == INVALID_HANDLE_VALUE) { 
		return 1;
	}

	//--------------------------------------------------
	//	        CRIAÇÃO DE THREADS SECUNDÁRIAS
	//--------------------------------------------------
	
	HANDLE threadDeMonitoramento, threadReceptora, threadDeLimpeza;
	DWORD threadDeMonitoramentoId, threadReceptoraId, threadDeLimpezaId;

	// Criação de uma thread de monitoramento do evento de encerramento
	threadDeMonitoramento = (HANDLE)_beginthreadex(NULL,
												   0,
												   monitorarEncerramento,
												   eventoDeEncerramento,
												   0,
												   (CAST_LPDWORD)&threadDeMonitoramentoId
	);

	if (threadDeMonitoramento) {
		std::cout << "[Processo de Gestão de Produção] Thread de monitoramento criada com sucesso. ID da thread: " << threadDeMonitoramentoId << std::endl;
	}
	else {
		std::cout << "[Processo de Gestão de Produção] Erro na criação da thread de monitoramento. Código: " << GetLastError() << std::endl;
	}

	// Criação de uma thread para receber mensagens do mailslot
	DadosRecepcao dadosRecepcao = {
		mailslotHandle,
		eventoDeEncerramento
	};
	
	threadReceptora =
    (HANDLE)_beginthreadex(
        NULL,
        0,
        receberMensagens,
        &dadosRecepcao,
        0,
        (CAST_LPDWORD)
        &threadReceptoraId
    );
		

	if (threadReceptora) {
		std::cout << "[Processo de Gestão de Produção] Thread recptora criada com sucesso. ID da thread: " << threadReceptoraId << std::endl;
	}
	else {
		std::cout << "[Processo de Gestão de Produção] Erro na criação da thread recptora. Código: " << GetLastError() << std::endl;
	}

	// Criação de uma thread dedicada ao monitoramento do comando de limpeza do console
	threadDeLimpeza = (HANDLE)_beginthreadex(
		nullptr,
		0,
		monitorarComandoDeLimpezaDoConsole,
		eventoDeLimparJanelaDaTarefaDeGestaoDaProducao,
		0,
		(CAST_LPDWORD) &threadDeLimpezaId
	);

	if (threadDeLimpeza == nullptr) {
		std::cout << "[Processo de Gestão de Produção] Erro ao criar thread de limpeza."
			<< std::endl;
	}
	
	//--------------------------------------------------
	//    FECHA OS HANDLES DOS EVENTOS E MAILSLOT
	//--------------------------------------------------

	unsigned int resultadoDeMonitorarEncerramento = monitorarEncerramento(eventoDeEncerramento);

	if (resultadoDeMonitorarEncerramento == 0) {
		std::cout << "[Processo de Gestão de Produção] Processo de gestão de produção encerrado idealmente." << std::endl;
	} else {
		std::cout << "[Processo de Gestão de Produção] O processo de gestão de produção foi encerrado com erro." << std::endl;
	}

	//--------------------------------------------------
	//	     	 AGUARDA O TÉRMINO DAS THREADS
	//			     E FECHA SEUS HANDLES
	//--------------------------------------------------
	if (threadReceptora != nullptr) {
    WaitForSingleObject(threadReceptora, INFINITE);
    CloseHandle(threadReceptora);
	}

	if (threadDeMonitoramento != nullptr) {
		WaitForSingleObject(threadDeMonitoramento, INFINITE);
		CloseHandle(threadDeMonitoramento);
	}

	CloseHandle(eventoDeEncerramento);
	CloseHandle(mailslotHandle);
	CloseHandle(eventoCriacaoMailSlot);

	return 0;
}
