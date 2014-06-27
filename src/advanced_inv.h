#ifndef _ADVANCED_INV_H_
#define _ADVANCED_INV_H_
#include "output.h"
#include <string>
#include <map>

typedef std::vector< std::list<item*> > itemslice;

class player;
class inventory;
class item;

struct point;

class AdvancedInventory {
  enum class Mode {
    Area, Linear
  };

  enum class SelectedPane {
    FarLeft = 0, Left = 1, Right = 2, FarRight = 3, None = -1
  };

  enum class SortRule {
    Unspecified = -1, Unsorted = 0, Name, Weight, Volume, Charges
  };

  class Pane {
    friend class AdvancedInventory;

    class ItemCompare {
      SortRule _rule;
      bool _ascending;
    public:
      ItemCompare(SortRule rule, bool ascending = true) : _rule(rule), _ascending(ascending) { }
      bool operator()(const std::list<item *> &, const std::list<item *> &) const;
      bool operator()(const std::pair<const std::list<item *> &, Pane *> &, const std::pair<const std::list<item *> &, Pane *> &) const;
    };

  protected:
    std::string _identifier;
    size_t _cursor = 0;
    int _maxVolume, _maxWeight;

    SortRule _sortRule = SortRule::Unsorted;
    bool _sortAscending = true;
    std::string _chevrons = "[ ]";

    std::vector<std::list<item*>> _stackedItems;
    bool _dirtyStack = true;

    void printItem (WINDOW *, const std::list<item *> &, bool active, bool highlighted);

    void clampCursor () { if (_cursor >= stackedItems().size()) _cursor = 0; }

  public:
    Pane(std::string id, int mV, int mW) : _identifier(id), _maxVolume(mV), _maxWeight(mW) { }
    virtual ~Pane() { }

    virtual int volume () const = 0;
    virtual int weight () const = 0;

    virtual int maxVolume () const { return _maxVolume; }
    virtual int maxWeight () const { return _maxWeight; }

    virtual int freeVolume () const { return maxVolume() - volume(); }
    virtual int freeWeight () const { return maxWeight() - weight(); }

    std::string chevrons () const { return _chevrons; }
    void chevrons (const std::string &c) { _chevrons = c; }

    SortRule sortRule () const { return _sortRule; }
    void sortRule (SortRule sortRule) { if (_sortRule == sortRule) _sortAscending = !_sortAscending; _sortRule = sortRule; }

    const std::vector<std::list<item *>> &stackedItems () {
      if (_dirtyStack)
        restack();

      return _stackedItems;
    }

    void draw (WINDOW *, bool active);

    void up () { if (_cursor == 0) _cursor = stackedItems().size(); _cursor--; }
    void down () { _cursor++; if (_cursor == stackedItems().size()) _cursor = 0; }

    void pageUp (size_t rows) { _cursor -= rows; clampCursor(); }
    void pageDown (size_t rows) { _cursor += rows; clampCursor(); }

    bool canTakeItem (const item *, size_t &count);
    const item *itemAtCursor(size_t &) const;

    void popItemAtCursor ();

    virtual void addItem (const item *) = 0;
    virtual void removeItem (const item *) = 0;

  private:
    virtual void restack ();
  };

  class InventoryPane : public Pane {
    inventory &_inv;

    void restack () override;
  public:
    InventoryPane(std::string id, inventory &inv, int mV, int mW) : Pane(id, mV, mW), _inv(inv) {};
    InventoryPane(std::string id, player *);

    int volume () const override;
    int weight () const override;

    void addItem (const item *) override;
    void removeItem (const item *) override;
  };

  class ItemVectorPane : public Pane {
    std::vector<item> &_inv;

    void restack () override;
  public:
    ItemVectorPane(std::string id, std::vector<item> &inv, int mV, int mW) : Pane(id, mV, mW), _inv(inv) { }

    int volume () const override;
    int weight () const override;

    void addItem (const item *) override;
    void removeItem (const item *) override;
  };

  class AggregatePane : public Pane {
    std::map<std::string, Pane *> _panes;

    std::vector<Pane *> _stackedItemsPane;

    void restack () override;
  public:
    AggregatePane(std::string id, std::map<std::string, Pane *> panes) : Pane(id, -1, -1), _panes(panes) { }

    int volume () const override;
    int weight () const override;

    int maxVolume ();
    int maxWeight ();

    int maxVolume () const override;
    int maxWeight () const override;

    void addItem (const item *) override;
    void removeItem (const item *) override;
  };

  std::map<std::string, Pane *> _panes;

  std::map<SelectedPane, Pane *> _selections;
  SelectedPane _selectedPane = SelectedPane::Left;
  SelectedPane _unselectedPane = SelectedPane::Right;

  Mode _mode;

  AdvancedInventory(player *, player * = nullptr);

  void displayHead (WINDOW *);
  void displayPanes (WINDOW *, WINDOW *);

  bool selectPane (std::string id, SelectedPane = SelectedPane::None);

  int w_width, w_height;

  void right ();
  void left ();

  void swapFocus () { std::swap(_selectedPane, _unselectedPane); }

  Pane *selectedPane () const { return _selections.at(_selectedPane); }
  Pane *unselectedPane () const { return _selections.at(_unselectedPane); }

  void drawIndicatorAtom (WINDOW *, std::string, point, bool) const;
  void drawAreaIndicator (WINDOW *, bool) const;

  bool moveItem ();
  bool moveAll ();
  void moveALL ();

  void sort ();

 public:
  static void display (player *, player * = nullptr);
};

struct advanced_inv_area {
    int id;
    int hscreenx;
    int hscreeny;
    int offx;
    int offy;
    int x;
    int y;
    std::string name;
    std::string shortname;
    bool canputitems;
    vehicle *veh;
    int vstor;
    int size;
    std::string desc;
    int volume, weight;
    int max_size, max_volume;
};

// see item_factory.h
class item_category;

// for printing items in environment
struct advanced_inv_listitem {
    int idx;
    int area;
    item *it;
    std::string name;
    std::string name_without_prefix;
    bool autopickup;
    int stacks;
    int volume;
    int weight;
    const item_category *cat;
};

class advanced_inventory_pane
{
    public:
        int pos;
        int area, offx, offy, size, vstor;  // quick lookup later
        int index, max_page, max_index, page;
        std::string area_string;
        int sortby;
        int issrc;
        vehicle *veh;
        WINDOW *window;
        std::vector<advanced_inv_listitem> items;
        int numcats;
        std::string filter;
        bool recalc;
        bool redraw;
        std::map<std::string, bool> filtercache;
        advanced_inventory_pane() {
            offx = 0;
            offy = 0;
            size = 0;
            vstor = -1;
            index = 0;
            max_page = 0;
            max_index = 0;
            page = 0;
            area_string =  _("Initializing...");
            sortby = 1;
            issrc = 0;
            veh = NULL;
            window = NULL;
            items.clear();
            numcats = 0;
            filter = "";
            filtercache.clear();
        }
};

class advanced_inventory
{
    public:
        player *p;

        const int head_height;
        const int min_w_height;
        const int min_w_width;
        const int max_w_width;
        const int left;
        const int right;
        const int isinventory;
        const int isall;
        const int isdrag;

        bool checkshowmsg;
        bool showmsg;
        bool inCategoryMode;

        int itemsPerPage;
        int w_height;
        int w_width;

        int headstart;
        int colstart;

        //    itemsPerPage=getmaxy(left_window)-ADVINVOFS;
        // todo: awaiting ui::menu // last_tmpdest=-1;
        bool exit;// = false;
        bool redraw;// = true;
        bool recalc;// = true;
        int lastCh;// = 0;

        int src;// = left; // the active screen , 0 for left , 1 for right.
        int dest;// = right;
        bool examineScroll;// = false;
        bool filter_edit;

        advanced_inventory_pane panes[2];
        advanced_inv_area squares[12];

        advanced_inventory() :
            head_height(5),
            min_w_height(10),
            min_w_width(FULL_SCREEN_WIDTH),
            max_w_width(120),
            left(0),
            right(1),
            isinventory(0),
            isall(10),
            isdrag(11) {
        }
        bool move_all_items();
        void display(player *pp);
        void print_items(advanced_inventory_pane &pane, bool active);
        void recalc_pane(int i);
        void redraw_pane(int i);
        void init(player *pp);
    private:
        bool isDirectionalDragged(int area1, int area2);
};

#endif
