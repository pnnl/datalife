import plotly.graph_objects as go


def plot_sanke(source, target, value):
    link = dict(source = source, target = target, value = value)
    data = go.Sankey(link = link)

    fig = go.Figure(data)

    fig.show()

    # If you need to save this file as a standalong html file:
    fig.write_html("../diagrams/sankey-diagram-plotly1.html")


def main():
    source = [0, 0, 1, 1, 0]
    target = [2, 3, 4, 5, 4]
    value = [8, 2, 2, 8, 4]
    plot_sanke(source, target, value)

if __name__ == "__main__":
    main()