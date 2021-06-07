const jimp = require('jimp')
const fs = require('fs')
const config = require('./config.json')
let image_colors = {}
fs.readdir(config['images-folder'], (err, file) => {
    file.forEach((element)=>{
        if (element.includes(".jpg") || element.includes(".png")) {
            console.log(config['images-folder'] + element)
            count_colors(config['images-folder']  + element)
        }
    })
 
})
let count_colors = (file) => {
    jimp.read(file, (err, img) => {
        for (let i = 0; i < img.getWidth(); i++) {
            for (let j = 0; j < img.getHeight(); j++) {
                let col = img.getPixelColor(i, j).toString()
                if (image_colors[col] == undefined) {
                    image_colors[col] = 0
                }
                else {
                    image_colors[col]++;
                }
            }
        }
        let number = 0;
        Object.entries(image_colors).forEach(([key, value]) => {
            number++;
        })
        console.log(file)
        console.log(number)
    })
}