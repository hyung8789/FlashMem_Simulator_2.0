# FlashMem_Simulator_2.0

<img src="/res/pgb-flashmemory.png" width="200" height="133"><br></br>
NAND Flash Memory Simulater for Block Mapping Method with 2 Types of Mapping Table, Hybrid Mapping Method (BAST : Block Associative Sector Translation - 1:2 Block level mapping with Dynamic Table)

	1) Static Table : Mapping table initially all correspond 1 : 1 (Logical Sector or Block Num -> Physical Sector or Block Num)
	2) Dynamic Table : Mapping table initially all empty (non-assigned)

※ Additional implementation

	- Garbage Collecter
		=> Because the cost of erasing operations is greater than reading and writing, 
		freeing up space for recording during idle time or through garbage collection at an appropriate time.

	- Trace function
		=> Performance test through write operation by specific pattern.	
<br></br>

<h3><strong>< Build Option ></strong></h3><br>

- Modify this predefined macro as you want (Refer to "Build_Options.h")

| Predefined Macro | For What |
|:---|:---|
| DEBUG_MODE | Trace all possible error situations (annotate to disable) |
| PAGE_TRACE_MODE | Trace wear-leveling per sector (page) for all physical sectors (pages) (annotate to disable) |
| BLOCK_TRACE_MODE | Trace wear-leveling per block for all physical blocks (annotate to disable) |
| SPARE_BLOCK_RATIO | The rate of spare blocks to be managed by the system for the total number of blocks (Blocks that cannot be record data directly) (Default : 0.08, 8%) |
| VICTIM_BLOCK_QUEUE_RATIO | Set the size of the Victim Block Queue to the ratio size of the total number of blocks in the generated flash memory (Default : 0.08, 8%) |
| VICTIM_BLOCK_DEBUG_MODE | Garbage Collector does not process the selected Victim Block after the end of the write operation (annotate to disable) |

<br></br> 

<h3><strong>< Normal Mode (with not use Any Mapping Method) Command List ></strong></h3>

| Command | Action |
|:---|:---|
| init x or i x | Create x MB Storage File |
| read PSN or r PSN | Read data at Physical Sector Num(PSN) Position |
| write PSN data or w PSN data | Write data at Physical Sector Num(PSN) Position |
| erase PBN or e PBN | Erase data at Physical Block Num(PBN) Position |
| change | Change Mapping Method |
| clrglobalcnt | Clear Global Flash Memory Operation Count |
| pbninfo PBN | Output meta information for all sectors of PBN |
| info | Output Current flash memory information |
| exit | Terminate Program |

<br></br>

<h3><strong>< Block Mapping Method, Hybrid Mapping (BAST) Method Command List ></strong></h1>

| Command | Action |
|:---|:---|
| init x or i x | Create x MB Storage File |
| read LSN or r LSN | Read data at Logical Sector Num(LSN) Position |
| write LSN data or w LSN data | Write data at Logical Sector Num(LSN) Position |
| change | Change Mapping Method |
| print | Print Mapping Table(LSN -> PSN or LBN -> PBN) |
| searchmode | Change Algorithm for finding empty sector(offset) in block |
| trace | Write test with "W [TAB] LSN" format file |
| clrglobalcnt | Clear Global Flash Memory Operation Count (No dependency on each other between Per Block or Per Sector trace count) |
| forcegc |  Force Garbage Collection to gain physical free space |
| ebqprint | Output Current Empty Block Queue (Only for Dynamic Table) |
| sbqprint | Output Current Spare Block Queue |
| vbqprint | Output Current Victim Block Queue |
| lbninfo LBN | Output meta information for all sectors of LBN |
| pbninfo PBN | Output meta information for all sectors of PBN |
| info | Output Current flash memory information |
| exit | Terminate Program after empty the Victim Queue by GC |

<br></br>

<h3><strong>< Bugs and needs to be improvement ></strong></h3><br>
	
- All known bugs fixed<br>
<br></br>

<h3><strong>< Trace Result ></strong></h3><br>

- <b>[You can see the trace results here.](https://github.com/hyung8789/FlashMem_Simulator_2.0/tree/master/Trace%20Result)</b><br>
<br></br>

<h3><strong>< References ></strong></h3><br>
	
- A survey of Flash Translation Layer : <br>https://www.sciencedirect.com/science/article/abs/pii/S1383762109000356<br>
- A SPACE-EFFICIENT FLASH TRANSLATION LAYER FOR COMPACTFLASH SYSTEMS : <br>https://ieeexplore.ieee.org/document/1010143<br>
- A Log Buffer-Based Flash Translation Layer Using Fully-Associative Sector Translation : https://dl.acm.org/doi/10.1145/1275986.1275990<br>
- Micron Technical Note - Garbage Collection : <br>https://www.micron.com/-/media/client/global/Documents/Products/Technical%20Note/NAND%20Flash/tn2960_garbage_collection_slc_nand.ashx<br>
- Micron Technical Note - Wear-Leveling Techniques in NAND Flash Devices :
<br>https://www.micron.com/-/media/client/global/documents/products/technical-note/nand-flash/tn2942_nand_wear_leveling.pdf<br>

