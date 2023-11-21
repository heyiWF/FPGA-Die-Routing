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

struct tdm_wire
{
    int id;
    int ratio;
    bool direction;     // 0: i -> j, 1: j -> i
    vector<int> netids; // nets that use this wire
};

struct arc
{
    int i;
    int j;
    bool is_tdm;
    int capacity;          // if SLL use this
    vector<tdm_wire> wire; // if TDM use this
};

struct die
{
    int id;
    int fpga;
    vector<node> nodes;
    vector<arc> arcs;
};

struct single_path
{
    int source;
    int sink;
    int uses;               // how many circuits are using this path in the net
    float routing_weight;   // routing weights may vary because using tdm wires with different ratios
    vector<int> path_route; // detail of this path (all the dies involved)
};

struct net
{
    int netid;
    int source;
    // vector<int> sinks;
    float max_routing_weight;
    vector<int> critical_path;
    vector<arc> arcs_set;
    vector<single_path> paths;
};

struct fpga
{
    int fpgaid;
    vector<int> dies;
};

// struct net, node, arc, die, fpga, tdm_wire, path

vector<net> nets;
vector<node> nodes;
vector<arc> arcs;
vector<die> dies;
vector<fpga> fpgas;
vector<int> path;
vector<int> nodes_in_die;
int num_fpga = 0;
int num_die = 0;
int num_node = 0;
int num_net = 0;

template <class T>
void clear_vector(vector<T> &vt)
{
    vector<T> vtTemp;
    vtTemp.swap(vt);
}

void clear_nodes_in_die()
{
    fill(nodes_in_die.begin(), nodes_in_die.end(), 0);
}

bool decr_order_sort(const net &net1, const net &net2)
{
    return net1.max_routing_weight > net2.max_routing_weight;
}
/*
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
            cout << j->i << " - " << j->j << " (" << j->capacity << ") " << boolalpha << j->is_tdm << endl;
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
}*/

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
            // cout << "new node {" << newnode.name << "}" << endl;
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
                newarc.is_tdm = false;
                newarc.capacity = num;
            }
            else
            {
                newarc.is_tdm = true;
                for (int i = 0; i < num / 2; i++)
                {
                    tdm_wire newwire;
                    newwire.id = i;
                    newwire.ratio = 4;
                    newwire.direction = 0;
                    newarc.wire.push_back(newwire);
                }
                for (int i = 0; i < num / 2; i++)
                {
                    tdm_wire newwire;
                    newwire.id = i;
                    newwire.ratio = 4;
                    newwire.direction = 1;
                    newarc.wire.push_back(newwire);
                }
            }
            // arcs.push_back(newarc);
            it1->arcs.push_back(newarc);
            it2->arcs.push_back(newarc);
            col++;
        }
        row++;
    }
}
/*
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
}*/

void update_tpm_net(arc &arc, tdm_wire &wire)
{
    for (auto &netid : wire.netids)
    {
        auto tpm_net = find_if(nets.begin(), nets.end(), boost::bind(&net::netid, _1) == netid);
        single_path critical_path;
        for (auto &route : tpm_net->paths)
        {
            if (route.path_route.size() <= 1)
                continue;
            for (int i = 0; i < route.path_route.size() - 1; i++)
            {
                if ((route.path_route[i] == arc.i && route.path_route[i + 1] == arc.j) || (route.path_route[i] == arc.j && route.path_route[i + 1] == arc.i))
                {
                    route.routing_weight += 4;
                    if (route.routing_weight > tpm_net->max_routing_weight)
                    {
                        critical_path = route;
                    }
                    break;
                }
            }
        }
        if (critical_path.routing_weight > tpm_net->max_routing_weight)
        {
            tpm_net->max_routing_weight = critical_path.routing_weight;
            tpm_net->critical_path = critical_path.path_route;
        }
    }
}

float calculate_distance(net &n, const vector<int> &path)
{
    float distance = 0;
    int point = 0;

    while (point < path.size() - 1)
    {
        int next = point + 1;
        for (auto d : dies)
        {
            if (d.id == path[point])
                die &die1 = d;
            if (d.id == path[next])
                die &die2 = d;
        }
        die &die1 = *find_if(dies.begin(), dies.end(), boost::bind(&die::id, _1) == path[point]);
        die &die2 = *find_if(dies.begin(), dies.end(), boost::bind(&die::id, _1) == path[next]);
        // arc arc;
        int i, j;
        for (i = 0; i < die1.arcs.size(); i++)
        {
            if ((die1.arcs[i].i == path[next] && die1.arcs[i].j == path[point]) || (die1.arcs[i].j == path[next] && die1.arcs[i].i == path[point]))
            {
                // arc = die.arcs[i];
                break;
            }
        }
        for (j = 0; j < die2.arcs.size(); j++)
        {
            if ((die2.arcs[j].i == path[next] && die2.arcs[j].j == path[point]) || (die2.arcs[j].j == path[next] && die2.arcs[j].i == path[point]))
            {
                // arc = die.arcs[i];
                break;
            }
        }
        bool is_recorded = false;
        for (auto const &recorded_arc : n.arcs_set)
        {
            if ((die1.arcs[i].i == recorded_arc.i && die1.arcs[i].j == recorded_arc.j) || (die1.arcs[i].i == recorded_arc.j && die1.arcs[i].j == recorded_arc.i))
            {
                is_recorded = true;
                break;
            }
        }
        bool tdm_direction = die1.arcs[i].i < die1.arcs[i].j ? 0 : 1;
        int min_ratio;
        for (int a = 0; a < die1.arcs[i].wire.size(); a++)
        {
            if (die1.arcs[i].wire[a].direction == tdm_direction)
            {
                min_ratio = die1.arcs[i].wire[a].ratio;
                break;
            }
        }
        if (die1.arcs[i].is_tdm)
        {
            int min_index = 0;
            for (int index = 0; index < die1.arcs[i].wire.size(); index++)
            {
                if (die1.arcs[i].wire[index].ratio < min_ratio && die1.arcs[i].wire[index].direction == tdm_direction)
                {
                    min_index = index;
                    min_ratio = die1.arcs[i].wire[index].ratio;
                }
            }
            tdm_wire &min_wire = die1.arcs[i].wire[min_index];
            tdm_wire &min_wire2 = die2.arcs[j].wire[min_index];
            distance += 0.5 * (1 + 2 * min_ratio);
            if (!is_recorded)
            {
                min_wire.netids.push_back(n.netid);
                min_wire2.netids.push_back(n.netid);
                /*if ((min_wire.netids.size() - min_wire.ratio) >= 4)
                {
                    min_wire.ratio += 4;
                    min_wire2.ratio += 4;
                    update_tpm_net(die1.arcs[i], min_wire);
                }*/
                if (min_wire.netids.size() % 4 == 0)
                {
                    min_wire.ratio = min_wire.netids.size();
                    min_wire2.ratio = min_wire2.netids.size();
                    update_tpm_net(die1.arcs[i], min_wire);
                }
                else
                {
                    min_wire.ratio = (min_wire.netids.size() / 4 + 1) * 4;
                    min_wire2.ratio = (min_wire2.netids.size() / 4 + 1) * 4;
                }
                n.arcs_set.push_back(die1.arcs[i]);
            }
        }
        else
        {
            distance++;
            if (!is_recorded)
            {
                die1.arcs[i].capacity--;
                n.arcs_set.push_back(die1.arcs[i]);
            }
        }
        point++;
    }
    return distance;
}

float bfs_find_path(die source_die, die sink_die, net &n, vector<int> &path)
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
            if (!u.arcs[arc].is_tdm && u.arcs[arc].capacity > 0) // sll
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
                        path.push_back(source_die.id);
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
            else // tdm
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
                        path.push_back(source_die.id);
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
    for (int i = 0; i < nodes_in_die.size(); i++)
    {
        if (nodes_in_die[i] == 0)
            continue;
        cout << n.source << " -> " << i;
        vector<int> route;
        die sink_die = *find_if(dies.begin(), dies.end(), boost::bind(&die::id, _1) == i);
        /*
        if (!n.paths.empty())
        {
            bool flag = false;
            for (auto &p : n.paths)
            {
                if (p.source == source_die.id && p.sink == i)
                {
                    cout << " [ ";
                    for (int i = p.path_route.size() - 1; i >= 0; i--)
                    {
                        cout << p.path_route[i] << " ";
                    }
                    cout << "]";
                    cout << " [" << p.routing_weight << "] <" << p.uses << ">" << endl;
                    flag = true;
                    break;
                }
            }
            if (flag)
                continue;
        }*/
        float distance;
        if (n.source != i)
        {
            distance = bfs_find_path(source_die, sink_die, n, route);
            if (distance == -1)
            {
                exit(-1);
            }
            if (distance > n.max_routing_weight)
            {
                n.max_routing_weight = distance;
                n.critical_path = route;
            }
        }
        else
        {
            distance = 0;
            route.push_back(n.source);
        }
        single_path newpath;
        newpath.source = n.source;
        newpath.sink = i;
        newpath.path_route = route;
        newpath.uses = nodes_in_die[i];
        newpath.routing_weight = distance;
        n.paths.push_back(newpath);
        cout << " [ ";
        for (int i = route.size() - 1; i >= 0; i--)
        {
            cout << route[i] << " ";
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
                // clear_vector(nets.back().sinks);
                clear_nodes_in_die();
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
            // node newnode = *it;
            // nets.back().sinks.push_back(newnode.die);
            nodes_in_die[it->die]++;
        }
    }
    route_net(nets.back());
    cout << "{" << nets.back().max_routing_weight << "}" << endl;
    // clear_vector(nets.back().sinks);
    float f = 0;
    for (auto i = nets.begin(); i != nets.end(); i++)
    {
        if (i->max_routing_weight > f)
        {
            f = i->max_routing_weight;
        }
    }
    cout << "Max routing weight: " << f << endl;
}

void sort_all_nets()
{
    sort(nets.begin(), nets.end(), decr_order_sort);
}

void route_all_nets()
{
    float f = 0;
    for (auto i = nets.begin(); i != nets.end(); i++)
    {
        route_net(*i);
        if (i->max_routing_weight > f)
            f = i->max_routing_weight;
    }
    cout << "Max routing weight: " << f << endl;
}

void file_output() // this function is (not yet) destructive!!
{
    sort_all_nets();
    cout << "design.route.out" << endl;
    ofstream output1;
    output1.open("design.route.out");
    if (!output1.is_open())
    {
        cout << "Error opening file";
        exit(1);
    }
    for (auto i = nets.begin(); i != nets.end(); i++)
    {
        output1 << "[" << i->netid << "]" << endl;
        // sort_net_paths(*i);
        single_path out_path;
        while (i->paths.size() > 0)
        {
            out_path = *i->paths.rbegin();
            for (int i = 0; i < out_path.uses; i++)
            {
                output1 << "[";
                for (auto k = out_path.path_route.rbegin(); k != out_path.path_route.rend() - 1; k++)
                {
                    output1 << *k << ",";
                }
                output1 << *(out_path.path_route.rend() - 1);
                output1 << "][" << out_path.routing_weight << "]" << endl;
            }
            i->paths.pop_back();
        }
        output1 << endl;
    }
    output1.close();
    cout << "design.tdm.out" << endl;
    ofstream output2;
    output2.open("design.tdm.out");
    if (!output2.is_open())
    {
        cout << "Error opening file";
        exit(1);
    }
    for (auto a : dies)
    {
        for (auto b : a.arcs)
        {
            if (b.is_tdm && b.i == a.id)
            {
                output2 << "[Die" << b.i << ",Die" << b.j << "]" << endl;
                for (auto c : b.wire)
                {
                    if (c.netids.size() == 0)
                        continue;
                    output2 << "[";
                    for (int d = 0; d < c.netids.size() - 1; d++)
                    {
                        output2 << c.netids[d] << ",";
                    }
                    output2 << c.netids[c.netids.size() - 1] << "] " << c.ratio << endl;
                }
                output2 << endl;
            }
        }
    }
    output1.close();
}

int count_lines()
{
    ifstream design_net;
    design_net.open("design.net");
    if (!design_net.is_open())
    {
        cout << "Error opening file";
        exit(1);
    }
    string s;
    int current_line = -1;
    int nets = 0;
    while (getline(design_net, s))
    {
        // if line contains "s"
        if (s.find("s") != -1)
            nets++;
        current_line++;
    }
    cout << "Total number of nets: " << nets << endl;
    cout << current_line << endl;
    return current_line;
}

int main()
{
    // count_lines();
    // return 0;
    cout << "Reading design.fpga.die!" << endl;
    read_fpga_die();
    nodes_in_die.resize(num_die);
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
    cout << "Finished. Writing to files... " << endl;
    file_output();
    return 0;
}