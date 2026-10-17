// UI texts: English and Brazilian Portuguese (picked from the Windows language, switchable in the title bar).
using System.Collections.Generic;
using System.Globalization;

namespace RebornSetup
{
    public static class L
    {
        public static bool Pt = CultureInfo.CurrentUICulture.TwoLetterISOLanguageName == "pt";
        public static string T(string key) => (Pt ? PT : EN).TryGetValue(key, out var s) ? s : (EN.TryGetValue(key, out s) ? s : key);
        public static string F(string key, params object[] a) => string.Format(T(key), a);

        static readonly Dictionary<string, string> EN = new Dictionary<string, string>
        {
            ["subtitle"] = "Expansion pack for ARA: History Untold",
            ["tagline"] = "New nations and leaders, player-built railroads, quality-of-life upgrades and a fix for the whole-PC freeze. Pick what you want: every mod is independent.",
            ["install"] = "Install",
            ["modify"] = "Modify",
            ["uninstallAll"] = "Uninstall everything",
            ["installed_n"] = "{0} of {1} mods installed",
            ["disclaimer"] = "Community project. Not affiliated with Oxide Games or Microsoft.",
            ["step_license"] = "License",
            ["step_content"] = "Content",
            ["step_location"] = "Location",
            ["step_install"] = "Install",
            ["step_finish"] = "Done",
            ["license_title"] = "License agreement",
            ["license_sub"] = "Please read the terms before installing.",
            ["license_accept"] = "I have read and accept the terms",
            ["content_title"] = "Choose your content",
            ["content_sub"] = "Turn on the mods you want. Turning off an installed mod removes it.",
            ["location_title"] = "Game location",
            ["location_sub"] = "Project ARA: Reborn is installed into your copy of ARA: History Untold.",
            ["browse"] = "Browse…",
            ["game_found"] = "ARA: History Untold found",
            ["game_missing"] = "ARA: History Untold was not found in this folder. Choose the folder that contains \"bin\\AraHistoryUntold.exe\".",
            ["pick_folder"] = "Choose the ARA: History Untold folder",
            ["space_ok"] = "Free space on {0}: {1:N1} GB",
            ["space_need"] = "The nation mods need about 4.5 GB of free space on {0} during installation ({1:N1} GB free).",
            ["game_running"] = "ARA: History Untold is running. Close the game to continue.",
            ["summary"] = "Summary",
            ["sum_install"] = "Install",
            ["sum_update"] = "Update",
            ["sum_remove"] = "Remove",
            ["sum_nothing"] = "No changes selected.",
            ["back"] = "Back",
            ["next"] = "Next",
            ["apply"] = "Apply changes",
            ["install_now"] = "Install",
            ["uninstall_now"] = "Uninstall",
            ["close"] = "Close",
            ["cancel"] = "Cancel",
            ["working_title"] = "Installing",
            ["working_title_rm"] = "Applying changes",
            ["working_sub"] = "Please keep the game closed. The nation mods can take a few minutes.",
            ["installing"] = "Installing {0}",
            ["removing"] = "Removing {0}",
            ["step_of"] = "Step {0} of {1}",
            ["details"] = "Details",
            ["done_title"] = "Ready to play",
            ["done_sub"] = "Your changes were applied.",
            ["done_title_err"] = "Finished with errors",
            ["done_sub_err"] = "Some changes could not be applied. Nothing else was changed.",
            ["done_title_rm"] = "Uninstalled",
            ["done_sub_rm"] = "The mods were removed and the original game files were restored.",
            ["ok_installed"] = "Installed",
            ["ok_removed"] = "Removed",
            ["failed"] = "Failed",
            ["play"] = "Play ARA",
            ["open_log"] = "Open log",
            ["tip_newgame"] = "Railroads, Brazil and the Soviet Union add game content: enable them in the Mods list when you start a NEW game.",
            ["tip_verify"] = "Steam's \"Verify integrity of game files\" and game updates turn the mods off. Run this installer again afterwards.",
            ["st_installed"] = "Installed",
            ["st_update"] = "Update available",
            ["st_new"] = "Not installed",
            ["st_will_install"] = "Will be installed",
            ["st_will_update"] = "Will be updated",
            ["st_will_remove"] = "Will be removed",
            ["tag_dll"] = "Game DLL",
            ["tag_content"] = "New game required",
            ["tag_ptbr"] = "PT-BR",
            ["tag_narration"] = "Custom narration",
            ["opt_station"] = "3D Train Station model (needs Python 3)",
            ["opt_station_nopy"] = "3D Train Station model: Python 3 was not found",
            ["confirm_close"] = "Installation is in progress. Closing now may leave a mod half-installed. Close anyway?",
            ["confirm_uninstall"] = "Remove every Project ARA: Reborn mod and restore the original game files?",
            ["err_title"] = "Project ARA: Reborn",
            ["mb"] = "{0:N1} MB",
            ["temp_space"] = "+ 4.5 GB temporary",

            ["AraQoL.name"] = "AraQoL",
            ["AraQoL.desc"] = "Higher camera ceiling, map banners that follow their toggle, easier unit clicking and \"Upgrade All\" buttons.",
            ["CpuFix.name"] = "ARA CPU Fix",
            ["CpuFix.desc"] = "Stops the whole-PC freeze during end-of-turn processing, a common problem reported by the community.",
            ["Railroads.name"] = "Railroads",
            ["Railroads.desc"] = "Build railroads with real track visuals, plus the new Train Station improvement.",
            ["Brazil.name"] = "Brazil — Getúlio Vargas",
            ["Brazil.desc"] = "Estado Novo: +50% build production towards Industry and +5 to all citizen stats. Unique unit: Pracinhas. 3D leader model.",
            ["SovietUnion.name"] = "Soviet Union — Joseph Stalin",
            ["SovietUnion.desc"] = "Great Patriotic War: +25% land strength at home, +50% military craft rate, −50% war weariness. Unique unit: T-34. Looks: 1924 and 1945.",

            ["slide1.t"] = "Two new nations",
            ["slide1.d"] = "Lead Brazil with Getúlio Vargas or the Soviet Union with Joseph Stalin, each with their own feature, unique unit, AI personality and 3D leader.",
            ["slide2.t"] = "Custom narration",
            ["slide2.d"] = "Both leaders have their own introduction narration, in English and Brazilian Portuguese.",
            ["slide3.t"] = "Railroads",
            ["slide3.d"] = "Lay down real tracks between your cities and build Train Stations along the line.",
            ["slide4.t"] = "A smoother game",
            ["slide4.d"] = "A higher camera, \"Upgrade All\" buttons and a fix for the freeze during end-of-turn processing.",
        };

        static readonly Dictionary<string, string> PT = new Dictionary<string, string>
        {
            ["subtitle"] = "Pacote de expansão para ARA: History Untold",
            ["tagline"] = "Novas nações e líderes, ferrovias construídas pelo jogador, melhorias de qualidade de vida e uma correção para o travamento do PC. Escolha o que quiser: cada mod é independente.",
            ["install"] = "Instalar",
            ["modify"] = "Modificar",
            ["uninstallAll"] = "Desinstalar tudo",
            ["installed_n"] = "{0} de {1} mods instalados",
            ["disclaimer"] = "Projeto da comunidade. Sem vínculo com a Oxide Games ou a Microsoft.",
            ["step_license"] = "Licença",
            ["step_content"] = "Conteúdo",
            ["step_location"] = "Local",
            ["step_install"] = "Instalação",
            ["step_finish"] = "Concluído",
            ["license_title"] = "Contrato de licença",
            ["license_sub"] = "Leia os termos antes de instalar.",
            ["license_accept"] = "Li e aceito os termos",
            ["content_title"] = "Escolha o conteúdo",
            ["content_sub"] = "Ligue os mods que quiser. Desligar um mod instalado remove ele.",
            ["location_title"] = "Local do jogo",
            ["location_sub"] = "O Project ARA: Reborn é instalado na sua cópia do ARA: History Untold.",
            ["browse"] = "Procurar…",
            ["game_found"] = "ARA: History Untold encontrado",
            ["game_missing"] = "O ARA: History Untold não foi encontrado nesta pasta. Escolha a pasta que contém \"bin\\AraHistoryUntold.exe\".",
            ["pick_folder"] = "Escolha a pasta do ARA: History Untold",
            ["space_ok"] = "Espaço livre em {0}: {1:N1} GB",
            ["space_need"] = "Os mods de nação precisam de cerca de 4,5 GB livres em {0} durante a instalação ({1:N1} GB livres).",
            ["game_running"] = "O ARA: History Untold está aberto. Feche o jogo para continuar.",
            ["summary"] = "Resumo",
            ["sum_install"] = "Instalar",
            ["sum_update"] = "Atualizar",
            ["sum_remove"] = "Remover",
            ["sum_nothing"] = "Nenhuma mudança selecionada.",
            ["back"] = "Voltar",
            ["next"] = "Avançar",
            ["apply"] = "Aplicar mudanças",
            ["install_now"] = "Instalar",
            ["uninstall_now"] = "Desinstalar",
            ["close"] = "Fechar",
            ["cancel"] = "Cancelar",
            ["working_title"] = "Instalando",
            ["working_title_rm"] = "Aplicando mudanças",
            ["working_sub"] = "Mantenha o jogo fechado. Os mods de nação podem levar alguns minutos.",
            ["installing"] = "Instalando {0}",
            ["removing"] = "Removendo {0}",
            ["step_of"] = "Etapa {0} de {1}",
            ["details"] = "Detalhes",
            ["done_title"] = "Pronto para jogar",
            ["done_sub"] = "Suas mudanças foram aplicadas.",
            ["done_title_err"] = "Concluído com erros",
            ["done_sub_err"] = "Algumas mudanças não puderam ser aplicadas. Nada além disso foi alterado.",
            ["done_title_rm"] = "Desinstalado",
            ["done_sub_rm"] = "Os mods foram removidos e os arquivos originais do jogo foram restaurados.",
            ["ok_installed"] = "Instalado",
            ["ok_removed"] = "Removido",
            ["failed"] = "Falhou",
            ["play"] = "Jogar ARA",
            ["open_log"] = "Abrir log",
            ["tip_newgame"] = "Ferrovias, Brasil e União Soviética adicionam conteúdo: ative-os na lista de Mods ao começar um jogo NOVO.",
            ["tip_verify"] = "A opção \"Verificar integridade dos arquivos\" da Steam e as atualizações do jogo desligam os mods. Depois disso, rode este instalador de novo.",
            ["st_installed"] = "Instalado",
            ["st_update"] = "Atualização disponível",
            ["st_new"] = "Não instalado",
            ["st_will_install"] = "Será instalado",
            ["st_will_update"] = "Será atualizado",
            ["st_will_remove"] = "Será removido",
            ["tag_dll"] = "DLL do jogo",
            ["tag_content"] = "Exige jogo novo",
            ["tag_ptbr"] = "PT-BR",
            ["tag_narration"] = "Narração própria",
            ["opt_station"] = "Modelo 3D da Estação de Trem (precisa do Python 3)",
            ["opt_station_nopy"] = "Modelo 3D da Estação de Trem: Python 3 não encontrado",
            ["confirm_close"] = "A instalação está em andamento. Fechar agora pode deixar um mod instalado pela metade. Fechar mesmo assim?",
            ["confirm_uninstall"] = "Remover todos os mods do Project ARA: Reborn e restaurar os arquivos originais do jogo?",
            ["err_title"] = "Project ARA: Reborn",
            ["mb"] = "{0:N1} MB",
            ["temp_space"] = "+ 4,5 GB temporários",

            ["AraQoL.name"] = "AraQoL",
            ["AraQoL.desc"] = "Teto da câmera mais alto, estandartes do mapa que obedecem ao botão, clique em unidades mais fácil e botões \"Melhorar Tudo\".",
            ["CpuFix.name"] = "ARA CPU Fix",
            ["CpuFix.desc"] = "Acaba com o travamento do PC inteiro durante o processamento do fim de turno, um problema comum relatado pela comunidade.",
            ["Railroads.name"] = "Ferrovias",
            ["Railroads.desc"] = "Construa ferrovias com trilhos de verdade no mapa e a nova melhoria Estação de Trem.",
            ["Brazil.name"] = "Brasil — Getúlio Vargas",
            ["Brazil.desc"] = "Estado Novo: +50% de produção para Indústria e +5 em todos os atributos dos cidadãos. Unidade única: Pracinhas. Líder em 3D.",
            ["SovietUnion.name"] = "União Soviética — Josef Stalin",
            ["SovietUnion.desc"] = "Grande Guerra Patriótica: +25% de força terrestre no próprio território, +50% de produção militar, −50% de cansaço de guerra. Unidade única: T-34. Visuais: 1924 e 1945.",

            ["slide1.t"] = "Duas novas nações",
            ["slide1.d"] = "Lidere o Brasil com Getúlio Vargas ou a União Soviética com Josef Stalin, cada um com habilidade própria, unidade única, personalidade de IA e líder em 3D.",
            ["slide2.t"] = "Narração própria",
            ["slide2.d"] = "Os dois líderes têm narração de apresentação própria, em inglês e português do Brasil.",
            ["slide3.t"] = "Ferrovias",
            ["slide3.d"] = "Construa trilhos de verdade entre suas cidades e Estações de Trem ao longo da linha.",
            ["slide4.t"] = "Um jogo mais fluido",
            ["slide4.d"] = "Câmera mais alta, botões \"Melhorar Tudo\" e uma correção para o travamento no fim do turno.",
        };

        public static string License => Pt ? LicensePt : LicenseEn;

        const string LicenseEn =
@"PROJECT ARA: REBORN
Author: Spartak (Arxz0)

1. WHAT YOU ARE INSTALLING
Project ARA: Reborn is a free, community-made collection of mods for ARA: History Untold. It is not affiliated with, endorsed by or supported by Oxide Games or Microsoft.

2. HOW THE MODS CHANGE YOUR GAME
• AraQoL, ARA CPU Fix and Railroads each replace one of the game's DLL files in the ""bin"" folder. The original DLL is kept next to it and is restored when you uninstall.
• Brazil and the Soviet Union install a shared leader base. It rebuilds some of the game's character packages from your own original Steam files and keeps the original bytes so they can be restored. It needs about 4.5 GB of free space while installing.
• Mod content is copied to Documents\My Games\Ara History Untold\Mods.
• This package contains no game files.

3. LICENSES
• Code (DLL sources, installers and tools): MIT License.
• Mod content (nation data, texts, portraits, emblems, textures and narration): Creative Commons Attribution-NonCommercial-ShareAlike 4.0 (CC BY-NC-SA 4.0).
• Third-party assets keep their own licenses:
  – Soviet marshal cap (Stalin 1945): adapted from ""Russian Imperial WW1 Army Cap"" by dawiderodawid (Sketchfab), CC BY 4.0: https://sketchfab.com/3d-models/russian-imperial-ww1-army-cap-d5d8d1c2636946a1adbaaeb2fe5dcd92
  – Leader narration: generated with Coqui XTTS-v2, under the Coqui Public Model License (non-commercial use). No real person's voice was cloned.
• Not covered: ARA: History Untold game files and content, which belong to their owners.
• ARA: History Untold © Microsoft. Project ARA: Reborn was created under Microsoft's ""Game Content Usage Rules"" (https://www.xbox.com/en-US/developers/rules) using assets from ARA: History Untold, and it is not endorsed by or affiliated with Microsoft.

4. NO WARRANTY
The software is provided ""as is"", without warranty of any kind. Use it at your own discretion and keep backups of your saves. The author is not liable for any damage or data loss.

5. GOOD TO KNOW
• Steam's ""Verify integrity of game files"" and game updates restore the original files and turn the mods off. Run this installer again afterwards.

Source code and updates: https://github.com/Arxz0/Project-ARA-Reborn";

        const string LicensePt =
@"PROJECT ARA: REBORN
Autor: Spartak (Arxz0)

1. O QUE VOCÊ ESTÁ INSTALANDO
O Project ARA: Reborn é uma coleção gratuita de mods feita pela comunidade para o ARA: History Untold. Não tem vínculo, aprovação nem suporte da Oxide Games ou da Microsoft.

2. COMO OS MODS ALTERAM O SEU JOGO
• AraQoL, ARA CPU Fix e Ferrovias trocam, cada um, uma DLL do jogo na pasta ""bin"". A DLL original fica guardada ao lado e volta quando você desinstala.
• Brasil e União Soviética instalam uma base de líderes compartilhada. Ela reconstrói alguns pacotes de personagens do jogo a partir dos seus próprios arquivos originais da Steam e guarda os bytes originais para restaurá-los. Precisa de cerca de 4,5 GB livres durante a instalação.
• O conteúdo dos mods é copiado para Documentos\My Games\Ara History Untold\Mods.
• Este pacote não contém arquivos do jogo.

3. LICENÇAS
• Código (fontes das DLLs, instaladores e ferramentas): Licença MIT.
• Conteúdo dos mods (dados das nações, textos, retratos, emblemas, texturas e narração): Creative Commons Atribuição-NãoComercial-CompartilhaIgual 4.0 (CC BY-NC-SA 4.0).
• Conteúdo de terceiros mantém a própria licença:
  – Quepe de marechal (Stalin 1945): adaptado de ""Russian Imperial WW1 Army Cap"", de dawiderodawid (Sketchfab), CC BY 4.0: https://sketchfab.com/3d-models/russian-imperial-ww1-army-cap-d5d8d1c2636946a1adbaaeb2fe5dcd92
  – Narração dos líderes: gerada com o Coqui XTTS-v2, sob a Coqui Public Model License (uso não comercial). Nenhuma voz de pessoa real foi clonada.
• Não coberto: arquivos e conteúdo do ARA: History Untold, que pertencem aos seus donos.
• ARA: History Untold © Microsoft. O Project ARA: Reborn foi criado sob as ""Game Content Usage Rules"" da Microsoft (https://www.xbox.com/en-US/developers/rules) usando conteúdo do ARA: History Untold, e não tem aprovação nem vínculo com a Microsoft.

4. SEM GARANTIA
O software é fornecido ""como está"", sem garantia de qualquer tipo. Use por sua conta e faça backup dos seus saves. O autor não se responsabiliza por danos ou perda de dados.

5. BOM SABER
• A opção ""Verificar integridade dos arquivos"" da Steam e as atualizações do jogo restauram os arquivos originais e desligam os mods. Depois disso, rode este instalador de novo.

Código-fonte e atualizações: https://github.com/Arxz0/Project-ARA-Reborn";
    }
}
