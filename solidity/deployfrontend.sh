rsync -r src/ docs/
rsync build/contracts/* docs/
git add .
git commit -m "Compile assets"
git push -u origin2 master
