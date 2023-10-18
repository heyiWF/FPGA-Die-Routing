#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <string>
#include <map>
#include <boost/bind.hpp>
#include <cstdlib>

using namespace std;

struct node
{
    string name;
    int die;
};

struct arc
{
    int i;
    int j;
    bool is_tpm;
    int capacity;
};

struct die
{
    int id;
    int fpga;
    vector<node> nodes;
    vector<arc> arcs;
};

struct net
{
    int netid;
    node source;
    vector<node> sinks;
};

struct fpga
{
    int fpgaid;
    vector<int> dies;
};

// struct net, node, arc, die, fpga

vector<net> nets;
vector<node> nodes;
vector<arc> arcs;
vector<die> dies;
vector<fpga> fpgas;
vector<int> path;
int num_fpga = 0;
int num_die = 0;
int num_node = 0;
int num_net = 0;

void test_print_all()
{
    cout << "Total number of FPGA: " << num_fpga << endl;
    cout << "Total number of die: " << num_die << endl;
    for (auto i = fpgas.begin(); i != fpgas.end(); i++)
    {
        cout << "FPGA " << i->fpgaid << ": ";
        for (auto j = i->dies.begin(); j != i->dies.end(); j++)
        {
            cout << *j << " ";
        }
        cout << endl;
    }

    for (auto i = dies.begin(); i != dies.end(); i++)
    {
        cout << "Die " << i->id << ": ";
        for (auto j = i->nodes.begin(); j != i->nodes.end(); j++)
        {
            cout << j->name << " ";
        }
        cout << endl;
    }

    for (auto i = dies.begin(); i != dies.end(); i++)
    {
        cout << "Die " << i->id << ": " << endl;
        for (auto j = i->arcs.begin(); j != i->arcs.end(); j++)
        {
            cout << j->i << " - " << j->j << " (" << j->capacity << ") " << boolalpha << j->is_tpm << endl;
        }
        cout << endl;
    }

    for(auto i = nets.begin(); i != nets.end(); i++)
    {
        cout << "Net " << i->netid << ": " << endl;
        cout << i->source.name << " -> ";
        for(auto j = i->sinks.begin(); j != i->sinks.end(); j++)
        {
            cout << j->name << " ";
        }
        cout << endl;
    }
}

void read_fpga_die()
{
    num_fpga = -1;
    // open file
    ifstream fpga_die;
    fpga_die.open("design.fpga.die");
    if (!fpga_die.is_open())
    {
        cout << "Error opening file";
        exit(1);
    }
    string s;
    while (getline(fpga_die, s))
    {
        int pos = s.find(":");
        if (pos != string::npos)
        {
            int fpga = stoi(s.substr(4, pos - 4));
        }
        fpgas.push_back(fpga());
        num_fpga++;
        fpgas.back().fpgaid = num_fpga;
        while (s.find("Die") != -1)
        {
            int pos = s.find("Die");
            int pos2 = -1;
            // find the next position of "Die"
            for (int i = pos + 1; i < s.length(); i++)
            {
                if (s[i] == 'D')
                {
                    pos2 = i;
                    break;
                }
            }
            die newdie;
            if (pos2 == -1)
            {
                newdie.id = stoi(s.substr(pos + 3));
                s = "";
            }
            else
            {
                newdie.id = stoi(s.substr(pos + 3, pos2 - pos - 4));
                s = s.substr(pos2);
            }
            newdie.fpga = num_fpga;
            fpgas.back().dies.push_back(newdie.id);
            dies.push_back(newdie);
            num_die++;
        }
    }
    num_fpga++;
}

void read_die_position()
{
    // open file
    ifstream die_position;
    die_position.open("design.die.position");
    if (!die_position.is_open())
    {
        cout << "Error opening file";
        exit(1);
    }
    string s;
    while (getline(die_position, s))
    {
        int pos = s.find(":");
        int dieid = -1;
        if (pos != string::npos)
        {
            dieid = stoi(s.substr(3, pos - 3));
        }
        auto it = find_if(dies.begin(), dies.end(), boost::bind(&die::id, _1) == dieid);
        s = s.substr(pos + 2);
        while (!s.empty())
        {
            int pos = s.find(" ");
            node newnode;
            newnode.name = s.substr(0, pos);
            cout << newnode.name << endl;
            newnode.die = dieid;
            it->nodes.push_back(newnode);
            nodes.push_back(newnode);
            num_node++;
            if (pos != -1)
                s = s.substr(pos + 1);
            else
                break;
        }
    }
}

void read_die_network()
{
    // open file
    ifstream die_network;
    die_network.open("design.die.network");
    if (!die_network.is_open())
    {
        cout << "Error opening file";
        exit(1);
    }
    string s;
    int row = 0;
    while (getline(die_network, s))
    {
        int num;
        // convert string to stringstream
        stringstream ss(s);
        int col = 0;
        while (ss >> num)
        {
            if (num == 0 || row > col)
            {
                col++;
                continue;
            }
            auto it1 = find_if(dies.begin(), dies.end(), boost::bind(&die::id, _1) == row);
            auto it2 = find_if(dies.begin(), dies.end(), boost::bind(&die::id, _1) == col);
            arc newarc;
            newarc.i = it1->id;
            newarc.j = it2->id;
            if (it1->fpga == it2->fpga)
            {
                newarc.is_tpm = false;
                newarc.capacity = num;
            }
            else
            {
                newarc.is_tpm = true;
                newarc.capacity = num;
            }
            arcs.push_back(newarc);
            it1->arcs.push_back(newarc);
            it2->arcs.push_back(newarc);
            col++;
        }
        row++;
    }
}

void read_net()
{
    // open file
    ifstream design_net;
    design_net.open("design.net");
    if (!design_net.is_open())
    {
        cout << "Error opening file";
        exit(1);
    }
    string s;
    int current_line = -1;
    while (getline(design_net, s))
    {
        current_line++;
        string name;
        string type;
        stringstream ss(s);
        ss >> name;
        ss >> type;
        if (type == "s")
        {
            net newnet;
            newnet.netid = current_line;
            auto it = find_if(nodes.begin(), nodes.end(), boost::bind(&node::name, _1) == name);
            newnet.source = *it;
            nets.push_back(newnet);
        }
        else if (type == "l")
        {
            auto it = find_if(nodes.begin(), nodes.end(), boost::bind(&node::name, _1) == name);
            nets.back().sinks.push_back(*it);
        }
    }
}

int main()
{
    cout << "Hello World!" << endl;

    cout << "Reading design.fpga.die!" << endl;
    read_fpga_die();
    cout << "Reading design.die.position!" << endl;
    read_die_position();
    cout << "Reading design.die.network!" << endl;
    read_die_network();
    cout << "Reading design.net!" << endl;
    read_net();

    test_print_all();
    return 0;
}