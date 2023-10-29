#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <string>
#include <queue>
#include <map>
#include <boost/bind.hpp>
#include <cstdlib>

using namespace std;

struct node
{
    string name;
    int die;
};

struct tpm_wire
{
    int id;
    int ratio;
    vector<int> netids; // nets that use this wire
};

struct arc
{
    int i;
    int j;
    bool is_tpm;
    int capacity;          // if SLL use this
    vector<tpm_wire> wire; // if TPM use this
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
    int source;
    vector<int> sinks;
    float max_routing_weight;
    vector<int> critical_path;
    vector<arc> arcs_set;
};

struct fpga
{
    int fpgaid;
    vector<int> dies;
};

struct path
{
    int uses;                 // how many circuits are using this path
    float routing_weight;     // routing weights may vary because using tdm wires with different ratios
    vector<int> using_netids; // all nets that contain this path
    vector<int> path_route;   // detail of this path (all the dies involved)
};

// struct net, node, arc, die, fpga, tpm_wire, path

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

    for (auto i = nets.begin(); i != nets.end(); i++)
    {
        cout << "Net " << i->netid << ": " << endl;
        cout << i->source << " -> ";
        for (auto j = i->sinks.begin(); j != i->sinks.end(); j++)
        {
            cout << *j << " ";
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
        try
        {
            s = s.substr(pos + 1);
        }
        catch (const std::out_of_range &e)
        {
            s = "";
        }
        while (!s.empty())
        {
            int pos = s.find(" ");
            node newnode;
            newnode.name = s.substr(0, pos);
            newnode.name.erase(newnode.name.find_last_not_of(" \r\n") + 1);
            if (newnode.name == "")
                break;
            cout << "new node {" << newnode.name << "}" << endl;
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
                for (int i = 0; i < num; i++)
                {
                    tpm_wire newwire;
                    newwire.id = i;
                    newwire.ratio = 4;
                    newarc.wire.push_back(newwire);
                }
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
        cout << current_line << "\r";
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
            newnet.source = it->die;
            nets.push_back(newnet);
        }
        else if (type == "l")
        {
            auto it = find_if(nodes.begin(), nodes.end(), boost::bind(&node::name, _1) == name);
            nets.back().sinks.push_back(it->die);
        }
    }
}

int calculate_distance(net &n, const vector<int> &path)
{
    int distance = 0;
    int point = 0;

    while (point < path.size() - 1)
    {
        int next = point + 1;
        die *die = &*find_if(dies.begin(), dies.end(), boost::bind(&die::id, _1) == path[point]);
        arc *arc = nullptr;
        for (auto i = die->arcs.begin(); i != die->arcs.end(); i++)
        {
            if (i->i == path[next] || i->j == path[next])
            {
                arc = &*i;
                break;
            }
        }
        bool is_recorded = false;
        for (auto const &recorded_arc : n.arcs_set)
        {
            if ((arc->i == recorded_arc.i && arc->j == recorded_arc.j) || (arc->i == recorded_arc.j && arc->j == recorded_arc.i))
            {
                is_recorded = true;
                break;
            }
        }
        if (is_recorded)
        {
            point++;
            continue;
        }
        if (arc->is_tpm)
        {
            int min_ratio = arc->wire[0].ratio;
            tpm_wire *min_wire = &arc->wire[0];
            for (int i = 1; i < arc->wire.size(); i++)
            {
                if (arc->wire[i].ratio < min_ratio)
                {
                    min_ratio = arc->wire[i].ratio;
                    min_wire = &arc->wire[i];
                }
            }
            distance += min_ratio / 4;
            min_wire->netids.push_back(n.netid);
            if ((min_wire->netids.size() - min_wire->ratio) >= 4)
            {
                min_wire->ratio += 4;
            }
            n.arcs_set.push_back(*arc);
        }
        else
        {
            distance++;
            arc->capacity--;
            n.arcs_set.push_back(*arc);
        }
        point++;
    }
    return distance + 1;
}

int bfs_find_path(die source_die, die sink_die, net &n, vector<int> &path)
{
    vector<bool> visited(num_die, false);
    map<int, int> parent; // die and its parent
    parent[source_die.id] = source_die.id;
    queue<die> q;
    q.push(source_die);
    visited[source_die.id] = true;
    while (!q.empty())
    {
        die u = q.front();
        q.pop();
        for (int arc = 0; arc < u.arcs.size(); arc++)
        {
            int other = u.arcs[arc].i == u.id ? u.arcs[arc].j : u.arcs[arc].i;
            // cout << u.id << " -> " << other << " [" << u.arcs[arc].capacity << "]" << endl;
            if (!u.arcs[arc].is_tpm && u.arcs[arc].capacity > 0) // sll
            {
                if (!visited[other])
                {
                    if (other == sink_die.id)
                    {
                        path.push_back(other);
                        while (parent[u.id] != u.id)
                        {
                            path.push_back(u.id);
                            u = *find_if(dies.begin(), dies.end(), boost::bind(&die::id, _1) == parent[u.id]);
                        }
                        return calculate_distance(n, path);
                    }
                    else
                    {
                        parent[other] = u.id;
                        q.push(*find_if(dies.begin(), dies.end(), boost::bind(&die::id, _1) == other));
                        visited[other] = true;
                    }
                }
            }
            else // tpm
            {
                if (!visited[other])
                {
                    if (other == sink_die.id)
                    {
                        path.push_back(other);
                        while (parent[u.id] != u.id)
                        {
                            path.push_back(u.id);
                            u = *find_if(dies.begin(), dies.end(), boost::bind(&die::id, _1) == parent[u.id]);
                        }
                        return calculate_distance(n, path);
                    }
                    else
                    {
                        parent[other] = u.id;
                        q.push(*find_if(dies.begin(), dies.end(), boost::bind(&die::id, _1) == other));
                        visited[other] = true;
                    }
                }
            }
        }
    }

    return -1;
}

void route_net(net &n)
{
    cout << "Routing net " << n.netid << endl;
    die source_die = *find_if(dies.begin(), dies.end(), boost::bind(&die::id, _1) == n.source);
    for (auto i = n.sinks.begin(); i != n.sinks.end(); i++)
    {
        cout << n.source << " -> " << *i;
        vector<int> path;
        die sink_die = *find_if(dies.begin(), dies.end(), boost::bind(&die::id, _1) == *i);
        if (n.source == *i)
        {
            cout << " [ " << n.source << " ] [0]" << endl;
            continue;
        }
        int distance = bfs_find_path(source_die, sink_die, n, path);
        if (distance == -1)
        {
            exit(-1);
        }
        if (distance > n.max_routing_weight)
        {
            n.max_routing_weight = distance;
        }
        path.push_back(n.source);
        cout << " [ ";
        for (int i = path.size() - 1; i >= 0; i--)
        {
            cout << path[i] << " ";
        }
        cout << "]";
        cout << " [" << distance << "]" << endl;
    }
}

void read_and_route_net()
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
    bool flag = false;
    while (getline(design_net, s))
    {
        current_line++;
        cout << current_line << "\r";
        string name;
        string type;
        stringstream ss(s);
        ss >> name;
        ss >> type;
        if (type == "s")
        {
            if (flag)
            {
                route_net(nets.back());
                cout << "{" << nets.back().max_routing_weight << "}" << endl;
            }
            net newnet;
            newnet.netid = current_line;
            auto it = find_if(nodes.begin(), nodes.end(), boost::bind(&node::name, _1) == name);
            newnet.source = it->die;
            newnet.max_routing_weight = 0;
            nets.push_back(newnet);
            flag = true;
        }
        else if (type == "l")
        {
            auto it = find_if(nodes.begin(), nodes.end(), boost::bind(&node::name, _1) == name);
            node newnode = *it;
            if (find(nets.back().sinks.begin(), nets.back().sinks.end(), it->die) == nets.back().sinks.end()) // && it->die != nets.back().source)
            {
                nets.back().sinks.push_back(newnode.die);
            }
        }
    }
    route_net(nets.back());
    cout << "{" << nets.back().max_routing_weight << "}" << endl;
}

void route_all_nets()
{
    for (auto i = nets.begin(); i != nets.end(); i++)
    {
        route_net(*i);
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
    /*
    cout << "Reading design.net!" << endl;
    read_net();

    test_print_all();
    */
    cout << "Routing all nets!" << endl;
    // route_all_nets();
    read_and_route_net();
    return 0;
}