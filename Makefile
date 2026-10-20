# Linux/macOS: make   (a janela grafica nativa so existe no Windows; aqui vale terminal + SVG)
simulador: *.cpp *.hpp
	g++ -std=c++17 -O2 -Wall *.cpp -o simulador
