#include "arrange.h"

// подключён через arrange.h

extern FILE* db_out;

// -------------- AI -------------------------------
static void chooseFilling(Border borders[4], int width, int height); // выбирает стратегию порядка проверки
static bool tryToStandShip(Ship* ship, Board* board, int start_x, int end_x, int start_y, int end_y);
static void swapBorders(Pair* a, Pair* b);

// 
static void DrawActiveShip_InArrangeWindow(WindowParametres *warr, int number, int size);
static void DrawMessage_InArrangeWindow(WINDOW* WIN, const char* msg);
static void EraseErrorMessage_InArrangeWindow(WINDOW*); // clear hardcoded line 12
static void colorizeCurrShip(WINDOW* WIN, Ship Ship);


bool arrangingShips_ByPlayer(CorePlayerGameData *playerGameData) {
	ShipsInfo *ships = &playerGameData->ships;
	Board *board = &playerGameData->board;
    // Объявление параметров создаваемых окон.
    WindowParametres WMain;
    WindowParametres WArrange;
    WindowParametres WShip;
    WindowParametres WHelp;
    initWindowsParametres(playerGameData, &WMain, &WArrange, &WShip, &WHelp);

    DrawMainWindow(&WMain);
    DrawTableWindow(&WShip);
    DrawDefaultArrangeWindow(&WArrange, ships);

    // Координаты перемещения курсора в WArrange->ptrWin для выбора корабля.
    int currShipSize = 0; 
    int currShipNumber = 0; // 1..number
    initCurrActiveShip_Arrange(ships, &currShipNumber, &currShipSize); // ?
    DrawActiveShip_InArrangeWindow(&WArrange, currShipNumber, currShipSize);

    enum actWind { ARRANGE = 1, SHIP = 2 };
    enum actWind active_window = ARRANGE; // Номер активного окна.
    int index; // Индекс выбранного корабля в массиве кораблей

    // Вспомогательный корабль для работы с полем ship.
    Ship* TmpShip = malloc(sizeof(Ship));
    clearTmpShip(TmpShip);

    bool isAllShipsStanding = FALSE;
    int key;

    while((key = getch()) != KEY_F(2) && isAllShipsStanding != TRUE){
		EraseErrorMessage_InArrangeWindow(WArrange.ptrWin);
        switch(key){
        	case KEY_LEFT:
        	case KEY_RIGHT:
        	case KEY_UP:
        	case KEY_DOWN:
	            switch (active_window){
	                case ARRANGE:
                        DrawShips_InArangeWindow(&WArrange, ships);
						changeActiveShip(ships, &currShipNumber, &currShipSize, key);
						DrawActiveShip_InArrangeWindow(&WArrange, currShipNumber, currShipSize);
                        reDrawStandingShips(WShip.ptrWin, board);
                        colorizeCurrShip(WShip.ptrWin, ships->Ships[getIndex(ships, currShipNumber, currShipSize)]);
	                    break;
	                case SHIP:
	                    changeShipCoordinates(TmpShip, board, key);
	                    reDrawStandingShips(WShip.ptrWin, board);
	                    DrawTmpShip(WShip.ptrWin, TmpShip, board);
	                	break;            
	            }
	            break;
	        case 9: // tab
	            switch(active_window){
	                case SHIP:
	                    changeTypeOfShip(TmpShip, board);
	                    reDrawStandingShips(WShip.ptrWin, board);	
	                    DrawTmpShip(WShip.ptrWin, TmpShip, board);
	                    break;
	                case ARRANGE:
	                	break;
	        	}
    		    break;
	        case '\n':
	            switch (active_window){
	                case ARRANGE:
	                    active_window = SHIP;
	                    index = getIndex(ships, currShipNumber, currShipSize);
                    	clearTmpShip(TmpShip);
	                    if (ships->Ships[index].stand == FALSE){
	                        InitPrimaryCoordinates(currShipSize, TmpShip, board);
                        }
	                    else {
	                    	deleteShipFromField(&ships->Ships[index], board);
	                    	makeShipTmp(&ships->Ships[index], TmpShip);
	                    }
                        reDrawStandingShips(WShip.ptrWin, board);
                        DrawTmpShip(WShip.ptrWin, TmpShip, board);
	                    break;

	                case SHIP:
	                    index = getIndex(ships, currShipNumber, currShipSize);
	                    if (checkShipBorders(TmpShip, board) == FALSE){
	                        DrawMessage_InArrangeWindow(WArrange.ptrWin, "Ships can`t stand near which other!");
						}
	                    else {
	                    	addShip(&ships->Ships[index], TmpShip);
	                    	refresh_ship_player_array(ships, board);
	                    	reDrawStandingShips(WShip.ptrWin, board);

							isAllShipsStanding = checkAllShipsStanding(ships);
							if (isAllShipsStanding == TRUE)
								DrawMessage_InArrangeWindow(WArrange.ptrWin,
								"All ships arranged!\n Press any key to start the baatle!\n");
	                    	active_window = ARRANGE;
	                    }
	                    break;
        		}
                break;
            case 27: // Esc
                switch (active_window){
                    case SHIP:
                        reDrawStandingShips(WShip.ptrWin, board);
	                    active_window = ARRANGE;
                        break;
                    case ARRANGE:
                        // /No any reaction/
                        break;
                }
                break;
    	}
	}
    free(TmpShip);

	// Закончили расстановку кораблей. Очищаем ненужные окна.
    delwin(WArrange.ptrWin);
    delwin(WMain.ptrWin);
    delwin(WShip.ptrWin);
    // delwin(WHelp->ptrWin); // WHelp не используется, поэтому и не удаляем

	fprintf(db_out, "exit from arraging!\n");

	return true; //TODO
}


void arrangingShips_ByComputer(CorePlayerGameData *playerGameData) {
	ShipsInfo *ships = &playerGameData->ships;
	Board *board = &playerGameData->board;

	int index = 0;
	Border borders[4]; // вляиет на очередь проверки участков на поле
	// всего делится на 4 участка, разбивается одной точкой
	while (index < getShipsNumber(ships)) {
		chooseFilling(borders, board->Width, board->Height);

		if (ships->Ships[index].stand == TRUE) // если предыдущий не был поставлен, то текущий ставим заново
			deleteShipFromField(&ships->Ships[index], board);
		for (int i = 0; i < 4; i++){
			Ship tmpShip;
			makeShipTmp(&ships->Ships[index], &tmpShip);
			tmpShip.type = rand() % 2;
			if (tryToStandShip(&tmpShip, board,
								borders[i].pair_x.first, borders[i].pair_x.second,
								borders[i].pair_y.first, borders[i].pair_y.second))
			{
				addShip(&ships->Ships[index], &tmpShip);
				break;
			}
		}

		if (ships->Ships[index].stand) // если смогли поставить
			index++;
		else
			index--;
	}
	// TODO delete this before release
	FILE* f = fopen("compBoard.txt", "w");
	for (int y = 0; y < board->Height; y++){
		for (int x = 0; x < board->Width; x++)
			fprintf(f, "%d ", board->field[y][x]);
		fprintf(f, "\n");
	}
	fflush(f);
	fclose(f);
}


bool tryToStandShip(Ship* thisShip, Board* board, int start_x, int end_x, int start_y, int end_y){
	for (int y = start_y; y < end_y; y++){
		for (int x = start_x; x < end_x; x++){
			if ((thisShip->type == HORIZONTAL) && (x + thisShip->size > board->Width))
				break;
			if ((thisShip->type == VERTICAL) && (y + thisShip->size > board->Height))
				break;
			thisShip->x = x;
			thisShip->y = y;
			if (checkShipBorders(thisShip, board)){
				standing_ship(thisShip, board);
				thisShip->stand = TRUE;
				return TRUE;
			}
		}
	}
	return FALSE;
}

void chooseFilling(Border borders[4], int width, int height){
	int start_x = rand() % width;
	int start_y = rand() % height;
	bool dir_x = rand() % 2;
	bool dir_y = rand() % 2;

	borders[0].pair_x.first = borders[2].pair_x.first = 0;
	borders[0].pair_x.second = borders[2].pair_x.second = start_x;
	borders[1].pair_x.first = borders[3].pair_x.first = start_x;
	borders[1].pair_x.second = borders[3].pair_x.second = width;

	borders[0].pair_y.first = borders[2].pair_y.first = 0;
	borders[0].pair_y.second = borders[2].pair_y.second = start_y;
	borders[1].pair_y.first = borders[3].pair_y.first = start_y;
	borders[1].pair_y.second = borders[3].pair_y.second = height;

	if (dir_x){
		swapBorders(&borders[0].pair_x, &borders[1].pair_x);
	}
	if (dir_y){
		swapBorders(&borders[2].pair_y, &borders[3].pair_y);
	}
}

void swapBorders(Pair* a, Pair* b){
	Pair tmp = *a;
	*a = *b;
	*b = tmp;
}

void DrawActiveShip_InArrangeWindow(WindowParametres* warr, int number, int size) {
    int ch = 219;
    int y = 6 + number*2;
    int x = 0;
    switch (size){
        case 4: x =  3; break;
        case 3: x = 12; break;
        case 2: x = 21; break;
        case 1: x = 30; break;
        default: x = 0;
    }
    wattron(warr->ptrWin, COLOR_PAIR(100));
    for (int i = 0; i < size+1; i++)
        mvwprintw(warr->ptrWin, y, x+i,"%c", ch);
    wrefresh(warr->ptrWin);
}

void DrawMessage_InArrangeWindow(WINDOW* WIN, const char* msg) {
    wattron(WIN, COLOR_PAIR(100));
    mvwprintw(WIN, 12 , 1, msg); // TODO from size of board
    wattron(WIN, COLOR_PAIR(2));
    box(WIN, 0, 0);
    wrefresh(WIN);
}

void EraseErrorMessage_InArrangeWindow(WINDOW* WIN) {
    wattron(WIN, COLOR_PAIR(100));
    for (int i = 0; i < WIN->_maxx; i++)
        mvwprintw(WIN, 12 , i, " ");
    wattron(WIN, COLOR_PAIR(2));
    box(WIN, 0, 0);
    wrefresh(WIN);
}

void colorizeCurrShip(WINDOW* WIN, Ship ship) {
    if (ship.stand == TRUE) {
        wattron(WIN, COLOR_PAIR(2));
        int rect = 254;
        int i = ship.y;
        int j = ship.x;

        switch(ship.type) {
            case VERTICAL:
                for (; i < ship.size + ship.y; i++) {
                    mvwprintw(WIN, i*2+3, j*2+4, "%c", rect);
                }
                break;
            case HORIZONTAL:
                for (; j < ship.size + ship.x; j++) {
                    mvwprintw(WIN, i*2+3, j*2+4, "%c", rect);
                }
                break;
            default:
                Stopif(true, "colorizeCurrShip(): swithc-case unexpected value");
        }
        wrefresh(WIN);
    }
}
