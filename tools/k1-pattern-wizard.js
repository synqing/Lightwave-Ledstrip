#!/usr/bin/env node
const inquirer = require('inquirer');
const fs = require('fs');

inquirer.prompt([
  { name: 'name', message: 'Pattern name:' },
  { name: 'author', message: 'Author:' },
  { name: 'description', message: 'Description:' }
]).then(answers => {
  fs.writeFileSync(`${answers.name}.cpp`, `// Pattern code for ${answers.name}`);
  fs.writeFileSync(`${answers.name}_card.json`, JSON.stringify({
    metadata: { name: answers.name, author: answers.author, description: answers.description }
  }, null, 2));
  console.log('Pattern scaffolded!');
}); 