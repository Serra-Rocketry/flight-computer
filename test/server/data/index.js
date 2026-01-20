async function getFiles() {
  const response = await fetch("api/files");
  return await response.json();
}

// Faz uma requisição ao backend de todos os arquivos:
getFiles().then(files => {
  // Pega uma referência a um template HTML referente à uma linha de uma tabela.
  const $template = document.querySelector("#rowTemplate");

  // E para cada arquivo, copia e popula o template com os dados desse arquivo,
  // juntando todas as cópias em $tableRows.
  const $tableRows = files.map(file => {
    const $tableRow = $template.content.cloneNode(true);

    const $rowName = $tableRow.querySelector(".rowTemplateName");
    $rowName.textContent = file.name;

    const $rowSize = $tableRow.querySelector(".rowTemplateSize");
    $rowSize.textContent = humanFileSize(file.size);

    const $openLink = $tableRow.querySelector(".rowTemplateOpen");
    const fileURL = new URL("/api/file", document.location);
    fileURL.searchParams.set("filename", file.name);
    $openLink.href = fileURL.href;

    // Esse chidren[0] é referente a uma idiossincracia de atribuir ouvintes de
    // eventos à templates. O objetivo é atribuir um para cada linha da
    // tabela, que irá reagir ao ser clicado.
    $tableRow.children[0].addEventListener("click", async (event) => {
      const $rowContainer = event.currentTarget;
      // No fim só nos importamos se for o botão de deletar arquivo que "estiver
      // na mira", mas isso facilita na hora de removermos a linha da tabela.
      if(!event.target.classList.contains("rowTemplateDelete"))
        return;
      
      // Se for, requisitamos ao servidor que delete o arquivo correspondente
      const message = await handleDelete(fileURL);
      console.log(message);
      // e removemos sua linha do DOM.
      $rowContainer.remove();
    });

    const $downloadLink = $tableRow.querySelector(".rowTemplateDownload");
    fileURL.searchParams.set("download", 1); // Reusando URL por praticidade.
    $downloadLink.href = fileURL.href;

    return $tableRow;
  });

  // Por fim, popule a tabela com essas linhas.
  document.querySelector("#tableBody").replaceChildren(...$tableRows);
})

// Roubei do StackOverflow lol.
function humanFileSize(size) {
  var i = size == 0 ? 0 : Math.floor(Math.log(size) / Math.log(1024));
  return +((size / Math.pow(1024, i)).toFixed(2)) * 1 +
    ' ' + ['B', 'kB', 'MB', 'GB', 'TB'][i];
}

async function handleDelete(fileURL) {
  const response = await fetch(fileURL, { method: "DELETE", });
  return await response.text();
}
